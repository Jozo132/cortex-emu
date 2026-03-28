/**
 * cortex_emu.js — Hand-written JavaScript interface for the cortex-emu WASM module.
 *
 * Loads the bare-metal WASM binary built with clang + wasm-ld (no Emscripten).
 * Works in both browser (fetch API) and Node.js (fs.readFileSync) environments.
 *
 * Usage (ES module / Node.js):
 *
 *   import CortexEmu from './cortex_emu.js'
 *   const emu = await new CortexEmu('./dist/cortex_emu.wasm').initialize()
 *   const handle = emu.create()
 *   emu.configure(handle, CortexEmu.CORE_M0, 0x08000000, 32768, 0x20000000, 2048, 48000000)
 *   for (let i = 0; i + 3 < fw.length; i += 4)
 *       emu.loadFlash(handle, i, fw.readUInt32LE(i))
 *   emu.reset(handle)
 *   emu.run(handle, 100000)
 *   console.log('PC =', emu.getReg(handle, 15).toString(16))
 *   emu.destroy(handle)
 *
 * Usage (browser, script tag):
 *
 *   <script type="module">
 *     import CortexEmu from './cortex_emu.js'
 *     const emu = await new CortexEmu('/dist/cortex_emu.wasm').initialize()
 *     ...
 *   </script>
 */

/** @typedef {number} EmuHandle  — integer pointer into WASM linear memory */

const isNodeRuntime = typeof process !== 'undefined' &&
    process.versions != null && process.versions.node != null

class CortexEmu {
    // ── Core type constants ────────────────────────────────────────────────
    static CORE_M0     = 0
    static CORE_M0PLUS = 1
    static CORE_M3     = 2
    static CORE_M4     = 3

    // ── Register index constants ───────────────────────────────────────────
    static REG_XPSR    = 16
    static REG_PRIMASK = 17
    static REG_CONTROL = 18
    static REG_MSP     = 19
    static REG_PSP     = 20

    /** @type {WebAssembly.Instance} */    // @ts-ignore
    _wasm = null
    /** @type {Record<string, Function>} */ // @ts-ignore
    _ex   = null   // shorthand for _wasm.exports

    /** @param {string} wasm_path  Path to cortex_emu.wasm */
    constructor(wasm_path = '') {
        this.wasm_path = wasm_path
    }

    // ── Initialization ─────────────────────────────────────────────────────

    /**
     * Load and instantiate the WASM module.
     * @param {string} [wasm_path] Override the path provided to the constructor.
     * @returns {Promise<this>}
     */
    initialize = async (wasm_path = '') => {
        if (this._wasm) return this   // already initialized

        const path = wasm_path || this.wasm_path || './dist/cortex_emu.wasm'
        let wasmBuffer

        if (!isNodeRuntime) {
            // Browser: fetch the wasm file
            const resp = await fetch(path + '?t=' + Date.now())
            wasmBuffer = await resp.arrayBuffer()
        } else {
            // Node.js: read from filesystem
            const {readFileSync} = await import('fs')
            const {resolve}      = await import('path')
            const {fileURLToPath, URL} = await import('url')

            let resolved = path
            if (path === './dist/cortex_emu.wasm') {
                try {
                    resolved = fileURLToPath(new URL('./dist/cortex_emu.wasm', import.meta.url))
                } catch (_) {
                    resolved = resolve(path)
                }
            }
            wasmBuffer = readFileSync(resolved)
        }

        // Minimal imports: the bare-metal WASM needs no JS runtime functions.
        // Extend wasmImports.env if you need debug callbacks.
        const wasmImports = {
            env: {
                // Optional: override for debug output from firmware UART
                stdout: (/** @type {number} */ charCode) => {
                    if (this.onStdout) this.onStdout(String.fromCharCode(charCode))
                },
            },
        }

        const module   = await WebAssembly.compile(wasmBuffer)
        const instance = await WebAssembly.instantiate(module, wasmImports)
        this._wasm = instance
        this._ex   = instance.exports

        // Reset the bump allocator so any previous allocations are freed
        this._ex.initialize()

        const required = ['cortex_emu_create', 'cortex_emu_destroy', 'cortex_emu_configure',
                          'cortex_emu_reset', 'cortex_emu_step', 'cortex_emu_run',
                          'cortex_emu_get_reg', 'cortex_emu_set_reg',
                          'cortex_emu_get_fault_count', 'cortex_emu_get_gpio_output']
        for (const fn of required) {
            if (!this._ex[fn]) throw new Error(`WASM export missing: ${fn}`)
        }

        return this
    }

    /** Alias for initialize(). */
    init   = (...args) => this.initialize(...args)
    /** Alias for initialize(). */
    create_module = (...args) => this.initialize(...args)

    /** Optional callback for UART stdout characters: (char: string) => void */
    onStdout = null

    // ── Direct WASM memory access ──────────────────────────────────────────

    /**
     * Return the WASM linear memory as a Uint8Array.
     * Re-acquired on every call because WASM memory can grow.
     * @returns {Uint8Array}
     */
    get memory() {
        return new Uint8Array(this._ex.memory.buffer)
    }

    /**
     * Return a DataView over the WASM linear memory.
     * @returns {DataView}
     */
    get view() {
        return new DataView(this._ex.memory.buffer)
    }

    // ── Lifecycle ──────────────────────────────────────────────────────────

    /**
     * Allocate a new emulator instance.
     * @returns {EmuHandle}  Integer pointer into WASM linear memory.
     */
    create() {
        return this._ex.cortex_emu_create()
    }

    /**
     * Free an emulator instance.
     * (With the bump allocator this is a no-op, but still good practice.)
     * @param {EmuHandle} handle
     */
    destroy(handle) {
        this._ex.cortex_emu_destroy(handle)
    }

    // ── Configuration ──────────────────────────────────────────────────────

    /**
     * Configure the emulator for a specific device.
     * @param {EmuHandle} handle
     * @param {number} core       CORE_M0 / CORE_M0PLUS / CORE_M3 / CORE_M4
     * @param {number} flashBase  Flash start address  (e.g. 0x08000000)
     * @param {number} flashSize  Flash size in bytes
     * @param {number} sramBase   SRAM start address   (e.g. 0x20000000)
     * @param {number} sramSize   SRAM size in bytes
     * @param {number} clockHz    System clock in Hz   (0 = default 48 MHz)
     */
    configure(handle, core, flashBase, flashSize, sramBase, sramSize, clockHz) {
        this._ex.cortex_emu_configure(handle, core, flashBase, flashSize,
                                       sramBase, sramSize, clockHz)
    }

    /**
     * Load a 32-bit word into flash at the given byte offset.
     * @param {EmuHandle} handle
     * @param {number}    offset  Byte offset from the start of flash
     * @param {number}    word    32-bit value (little-endian)
     */
    loadFlash(handle, offset, word) {
        this._ex.cortex_emu_load_flash(handle, offset, word)
    }

    /**
     * Load firmware from a Uint8Array or Buffer into flash.
     * @param {EmuHandle}          handle
     * @param {Uint8Array|Buffer}  bytes  Raw firmware bytes
     */
    loadFirmware(handle, bytes) {
        const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength)
        const words = Math.floor(bytes.length / 4)
        for (let i = 0; i < words; i++) {
            this._ex.cortex_emu_load_flash(handle, i * 4, view.getUint32(i * 4, true))
        }
        // Handle remaining bytes (partial last word)
        const rem = bytes.length % 4
        if (rem > 0) {
            let last = 0
            for (let i = 0; i < rem; i++) last |= bytes[words * 4 + i] << (i * 8)
            this._ex.cortex_emu_load_flash(handle, words * 4, last >>> 0)
        }
    }

    // ── Execution ──────────────────────────────────────────────────────────

    /**
     * Reset the CPU (reads SP / PC from the vector table).
     * @param {EmuHandle} handle
     */
    reset(handle) {
        this._ex.cortex_emu_reset(handle)
    }

    /**
     * Execute one instruction.
     * @param {EmuHandle} handle
     * @returns {number} Cycle count for this instruction (0 if halted/lockup).
     */
    step(handle) {
        return this._ex.cortex_emu_step(handle)
    }

    /**
     * Execute up to maxCycles.
     * @param {EmuHandle} handle
     * @param {number}    maxCycles
     * @returns {number} Actual cycles executed.
     */
    run(handle, maxCycles) {
        return this._ex.cortex_emu_run(handle, maxCycles)
    }

    // ── Registers ─────────────────────────────────────────────────────────

    /**
     * Read a CPU register.
     * @param {EmuHandle} handle
     * @param {number}    index  0-15 = R0-R15, 16+ = special (REG_XPSR, ...)
     * @returns {number}
     */
    getReg(handle, index) {
        return this._ex.cortex_emu_get_reg(handle, index) >>> 0
    }

    /**
     * Write a CPU register.
     * @param {EmuHandle} handle
     * @param {number}    index
     * @param {number}    value
     */
    setReg(handle, index, value) {
        this._ex.cortex_emu_set_reg(handle, index, value)
    }

    // ── Memory access (goes through peripheral bus) ────────────────────────

    read8(handle, addr)              { return this._ex.cortex_emu_read8 (handle, addr) >>> 0 }
    read16(handle, addr)             { return this._ex.cortex_emu_read16(handle, addr) >>> 0 }
    read32(handle, addr)             { return this._ex.cortex_emu_read32(handle, addr) >>> 0 }
    write8(handle, addr, val)        { this._ex.cortex_emu_write8 (handle, addr, val) }
    write16(handle, addr, val)       { this._ex.cortex_emu_write16(handle, addr, val) }
    write32(handle, addr, val)       { this._ex.cortex_emu_write32(handle, addr, val) }

    // ── GPIO ──────────────────────────────────────────────────────────────

    /**
     * Drive GPIO input pins.
     * @param {EmuHandle} handle
     * @param {number}    port   0=GPIOA, 1=GPIOB, ...
     * @param {number}    value  Bitmask of driven pins
     */
    setGpioInput(handle, port, value)  { this._ex.cortex_emu_set_gpio_input(handle, port, value) }

    /**
     * Read GPIO input state.
     * @param {EmuHandle} handle
     * @param {number}    port
     * @returns {number}
     */
    getGpioInput(handle, port)  { return this._ex.cortex_emu_get_gpio_input (handle, port) >>> 0 }

    /**
     * Read GPIO output state (firmware-driven pins).
     * @param {EmuHandle} handle
     * @param {number}    port
     * @returns {number}
     */
    getGpioOutput(handle, port) { return this._ex.cortex_emu_get_gpio_output(handle, port) >>> 0 }

    /** Read GPIO MODER register (M0/F0 layout). */
    getGpioMode(handle, port)   { return this._ex.cortex_emu_get_gpio_mode  (handle, port) >>> 0 }

    // ── ADC ───────────────────────────────────────────────────────────────

    /**
     * Set the ADC conversion result for all channels.
     * @param {EmuHandle} handle
     * @param {number}    value  12-bit result (0-4095)
     */
    setAdcValue(handle, value)              { this._ex.cortex_emu_set_adc_value(handle, value) }

    /**
     * Set the ADC conversion result for a specific channel.
     * @param {EmuHandle} handle
     * @param {number}    channel  0-15
     * @param {number}    value    12-bit result (0-4095)
     */
    setAdcChannelValue(handle, channel, value) {
        this._ex.cortex_emu_set_adc_channel_value(handle, channel, value)
    }

    // ── UART ──────────────────────────────────────────────────────────────

    /**
     * Inject a byte into the UART receive buffer.
     * @param {EmuHandle} handle
     * @param {number}    uartIdx  0=UART1, 1=UART2
     * @param {number}    data     Byte value
     */
    uartReceive(handle, uartIdx, data)  { this._ex.cortex_emu_uart_receive(handle, uartIdx, data) }

    /**
     * Read the next transmitted byte from the UART TX buffer.
     * @param {EmuHandle} handle
     * @param {number}    uartIdx
     * @returns {number}  Byte value, or -1 if the TX buffer is empty.
     */
    uartReadTx(handle, uartIdx)         { return this._ex.cortex_emu_uart_read_tx(handle, uartIdx) }

    /**
     * Count of bytes pending in the UART TX buffer.
     * @param {EmuHandle} handle
     * @param {number}    uartIdx
     * @returns {number}
     */
    uartTxPending(handle, uartIdx)      { return this._ex.cortex_emu_uart_tx_pending(handle, uartIdx) }

    // ── SPI / I2C ─────────────────────────────────────────────────────────

    setSpiRx(handle, data)              { this._ex.cortex_emu_set_spi_rx   (handle, data) }
    getSpiTx(handle)                    { return this._ex.cortex_emu_get_spi_tx   (handle) >>> 0 }
    i2cReceive(handle, data)            { this._ex.cortex_emu_i2c_receive  (handle, data) }
    getI2cTx(handle)                    { return this._ex.cortex_emu_get_i2c_tx   (handle) >>> 0 }

    // ── Interrupts & external events ──────────────────────────────────────

    /**
     * Set an IRQ pending in the NVIC.
     * @param {EmuHandle} handle
     * @param {number}    irqNum  IRQ number (0-based, i.e. exception number minus 16)
     */
    triggerIrq(handle, irqNum)          { this._ex.cortex_emu_trigger_irq(handle, irqNum) }

    /**
     * Trigger an EXTI line.
     * @param {EmuHandle} handle
     * @param {number}    line    EXTI line number (0-15)
     * @param {number}    rising  1 = rising edge, 0 = falling edge
     */
    triggerExti(handle, line, rising)   { this._ex.cortex_emu_trigger_exti(handle, line, rising) }

    // ── Timer injection ───────────────────────────────────────────────────

    /**
     * Inject a TIM3 input capture event (for sensorless hall emulation).
     * @param {EmuHandle} handle
     * @param {number}    period      Period count
     * @param {number}    pulseWidth  Pulse-width count
     */
    injectTim3Capture(handle, period, pulseWidth) {
        this._ex.cortex_emu_inject_tim3_capture(handle, period, pulseWidth)
    }

    // ── DMA ───────────────────────────────────────────────────────────────

    /** Execute one DMA transfer tick. */
    dmaStep(handle) { this._ex.cortex_emu_dma_step(handle) }

    // ── Debugger: breakpoints & watchpoints ───────────────────────────────

    /**
     * Add a code breakpoint at the given address.
     * @param {EmuHandle} handle
     * @param {number}    addr
     * @returns {number}  0 on success, -1 if the breakpoint table is full.
     */
    addBreakpoint(handle, addr)    { return this._ex.cortex_emu_add_breakpoint   (handle, addr) }
    removeBreakpoint(handle, addr) { return this._ex.cortex_emu_remove_breakpoint(handle, addr) }
    clearBreakpoints(handle)       { this._ex.cortex_emu_clear_breakpoints(handle) }
    setWatchpoint(handle, addr)    { this._ex.cortex_emu_set_watchpoint(handle, addr) }
    clearWatchpoint(handle)        { this._ex.cortex_emu_clear_watchpoint(handle) }

    // ── Status queries ────────────────────────────────────────────────────

    isHalted(handle)         { return !!this._ex.cortex_emu_is_halted        (handle) }
    isSleeping(handle)       { return !!this._ex.cortex_emu_is_sleeping      (handle) }
    isLockup(handle)         { return !!this._ex.cortex_emu_is_lockup        (handle) }
    isBreakpointHit(handle)  { return !!this._ex.cortex_emu_is_breakpoint_hit(handle) }
    isWatchTriggered(handle) { return !!this._ex.cortex_emu_is_watch_triggered(handle) }
    getLastPc(handle)        { return this._ex.cortex_emu_get_last_pc        (handle) >>> 0 }

    /**
     * Total number of CPU cycles executed.
     * @param {EmuHandle} handle
     * @returns {bigint}  64-bit unsigned count (returned as BigInt by WASM i64).
     */
    getTotalCycles(handle)   { return this._ex.cortex_emu_get_total_cycles   (handle) }

    getActiveException(handle) { return this._ex.cortex_emu_get_active_exception(handle) }

    setHalted(handle, halted) { this._ex.cortex_emu_set_halted(handle, halted ? 1 : 0) }

    // ── Diagnostics ───────────────────────────────────────────────────────

    getFaultCount(handle)          { return this._ex.cortex_emu_get_fault_count         (handle) }
    getFaultCode(handle)           { return this._ex.cortex_emu_get_fault_code          (handle) }
    getResetCount(handle)          { return this._ex.cortex_emu_get_reset_count         (handle) }
    getSystickFireCount(handle)    { return this._ex.cortex_emu_get_systick_fire_count  (handle) }
    getExceptionEntryCount(handle) { return this._ex.cortex_emu_get_exception_entry_count(handle) }
    getExceptionExitCount(handle)  { return this._ex.cortex_emu_get_exception_exit_count (handle) }

    // ── PC / register trace ───────────────────────────────────────────────

    /**
     * Get a PC trace entry (circular buffer, 0 = oldest).
     * @param {EmuHandle} handle
     * @param {number}    index
     * @returns {number}
     */
    getPcTrace(handle, index)          { return this._ex.cortex_emu_get_pc_trace  (handle, index) >>> 0 }
    getRegTrace(handle, entry, regIdx) { return this._ex.cortex_emu_get_reg_trace (handle, entry, regIdx) >>> 0 }

    // ── Shadow stack ──────────────────────────────────────────────────────

    getShadowStackSize(handle)         { return this._ex.cortex_emu_get_shadow_stack_size(handle) }
    getShadowStackPc(handle, index)    { return this._ex.cortex_emu_get_shadow_stack_pc  (handle, index) >>> 0 }
    getShadowStackSp(handle, index)    { return this._ex.cortex_emu_get_shadow_stack_sp  (handle, index) >>> 0 }

    // ── GPIO write trace ──────────────────────────────────────────────────

    gpioTraceStart(handle)              { this._ex.cortex_emu_gpio_trace_start(handle) }
    gpioTraceStop(handle)               { this._ex.cortex_emu_gpio_trace_stop (handle) }
    gpioTraceCount(handle)              { return this._ex.cortex_emu_gpio_trace_count(handle) }

    /**
     * Get a GPIO trace record field.
     * @param {EmuHandle} handle
     * @param {number}    index  Record index
     * @param {number}    field  0=cycle_lo, 1=cycle_hi, 2=port, 3=reg, 4=val
     * @returns {number}
     */
    gpioTraceGet(handle, index, field)  { return this._ex.cortex_emu_gpio_trace_get(handle, index, field) >>> 0 }

    // ── TIM1 effective output control ─────────────────────────────────────

    getTim1EffCcmr1(handle) { return this._ex.cortex_emu_get_tim1_eff_ccmr1(handle) >>> 0 }
    getTim1EffCcmr2(handle) { return this._ex.cortex_emu_get_tim1_eff_ccmr2(handle) >>> 0 }
    getTim1EffCcer(handle)  { return this._ex.cortex_emu_get_tim1_eff_ccer (handle) >>> 0 }

    // ── Raw memory buffer access ──────────────────────────────────────────

    /**
     * Return a Uint8Array view into the emulator's flat memory buffer.
     * Useful for direct inspection / manipulation.
     * @param {EmuHandle} handle
     * @returns {Uint8Array}
     */
    getMemoryView(handle) {
        const ptr  = this._ex.cortex_emu_get_mem_ptr  (handle) >>> 0
        const size = this._ex.cortex_emu_get_mem_size ()       >>> 0
        return new Uint8Array(this._ex.memory.buffer, ptr, size)
    }

    /**
     * Byte offset of the flash region inside the flat memory buffer.
     * @returns {number}
     */
    getFlashOffset() { return this._ex.cortex_emu_get_flash_offset() >>> 0 }

    /**
     * Byte offset of the SRAM region inside the flat memory buffer.
     * @returns {number}
     */
    getSramOffset()  { return this._ex.cortex_emu_get_sram_offset()  >>> 0 }
}

// ── Module exports ─────────────────────────────────────────────────────────
// Works in ESM (import), CJS (require), and browser (<script type=module>)

if (typeof module !== 'undefined' && module.exports) {
    module.exports = CortexEmu
} else {
    // @ts-ignore
    if (typeof globalThis !== 'undefined') globalThis.CortexEmu = CortexEmu
}

export default CortexEmu
