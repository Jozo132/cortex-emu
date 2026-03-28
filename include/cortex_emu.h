/*
 * cortex-emu — Portable ARM Cortex-M Emulator
 *
 * Supports: Cortex-M0, M0+, M3, M4 (ARMv6-M / ARMv7-M)
 * Features: Full Thumb/Thumb-2 ISA, NVIC, SysTick, GPIO, Timers,
 *           UART, SPI, I2C, DMA, EXTI, CRC, watchdogs, and more.
 *
 * Build targets:
 *   - Native C library (static/shared) via CMake
 *   - WebAssembly via bare-metal LLVM (clang + wasm-ld, see wasm/build.sh)
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef CORTEX_EMU_H
#define CORTEX_EMU_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Core type constants ─────────────────────────────────────────────── */
#define CORTEX_EMU_CORE_M0      0
#define CORTEX_EMU_CORE_M0PLUS  1
#define CORTEX_EMU_CORE_M3      2
#define CORTEX_EMU_CORE_M4      3

/* ── HardFault reason codes ──────────────────────────────────────────── */
#define CORTEX_EMU_FAULT_UNKNOWN        0
#define CORTEX_EMU_FAULT_INVALID_PC     1
#define CORTEX_EMU_FAULT_UNDEFINED_INSN 2
#define CORTEX_EMU_FAULT_STACK_OVERFLOW 3
#define CORTEX_EMU_FAULT_BAD_COND       4
#define CORTEX_EMU_FAULT_BAD_MISC       5
#define CORTEX_EMU_FAULT_UNIMPL_32      6

/* ── Register indices for get_reg / set_reg ──────────────────────────── */
#define CORTEX_EMU_REG_XPSR    16
#define CORTEX_EMU_REG_PRIMASK 17
#define CORTEX_EMU_REG_CONTROL 18
#define CORTEX_EMU_REG_MSP     19
#define CORTEX_EMU_REG_PSP     20

/* ── Memory layout constants ─────────────────────────────────────────── */
#define CORTEX_EMU_MEM_SIZE       786432  /* 768 KB */
#define CORTEX_EMU_FLASH_OFFSET   0x00000
#define CORTEX_EMU_SRAM_OFFSET    0x80000
#define CORTEX_EMU_PERIPH_OFFSET  0xA0000
#define CORTEX_EMU_SYSTEM_OFFSET  0xB0000

/* ── Opaque emulator handle ──────────────────────────────────────────── */
typedef struct cortex_emu cortex_emu_t;

/* ═══════════════════════════════════════════════════════════════════════
 *  Lifecycle
 * ═══════════════════════════════════════════════════════════════════════ */

/** Allocate and return a new emulator instance.  Returns NULL on failure. */
cortex_emu_t *cortex_emu_create(void);

/** Free an emulator instance. */
void cortex_emu_destroy(cortex_emu_t *emu);

/* ═══════════════════════════════════════════════════════════════════════
 *  Configuration
 * ═══════════════════════════════════════════════════════════════════════ */

/** Configure the emulator for a specific device.
 *  @param core       One of CORTEX_EMU_CORE_M0..M4.
 *  @param flash_base Flash start address (e.g. 0x08000000).
 *  @param flash_size Flash size in bytes.
 *  @param sram_base  SRAM start address (e.g. 0x20000000).
 *  @param sram_size  SRAM size in bytes.
 *  @param clock_hz   System clock in Hz (0 = default 48 MHz).
 */
void cortex_emu_configure(cortex_emu_t *emu, uint32_t core,
                           uint32_t flash_base, uint32_t flash_size,
                           uint32_t sram_base, uint32_t sram_size,
                           uint32_t clock_hz);

/** Load a 32-bit word into flash at the given byte offset. */
void cortex_emu_load_flash(cortex_emu_t *emu, uint32_t offset, uint32_t value);

/* ═══════════════════════════════════════════════════════════════════════
 *  Execution
 * ═══════════════════════════════════════════════════════════════════════ */

/** Reset the CPU (reads SP and PC from vector table). */
void cortex_emu_reset(cortex_emu_t *emu);

/** Execute one instruction.  Returns cycle count (0 if halted/lockup). */
uint32_t cortex_emu_step(cortex_emu_t *emu);

/** Execute up to max_cycles.  Returns actual cycles executed. */
uint32_t cortex_emu_run(cortex_emu_t *emu, uint32_t max_cycles);

/* ═══════════════════════════════════════════════════════════════════════
 *  Registers
 * ═══════════════════════════════════════════════════════════════════════ */

/** Read a register (0-15 = R0-R15, 16+ = special, see CORTEX_EMU_REG_*). */
uint32_t cortex_emu_get_reg(const cortex_emu_t *emu, uint32_t index);

/** Write a register. */
void cortex_emu_set_reg(cortex_emu_t *emu, uint32_t index, uint32_t value);

/* ═══════════════════════════════════════════════════════════════════════
 *  Memory Access (address-space, goes through peripherals)
 * ═══════════════════════════════════════════════════════════════════════ */

uint32_t cortex_emu_read8(cortex_emu_t *emu, uint32_t addr);
uint32_t cortex_emu_read16(cortex_emu_t *emu, uint32_t addr);
uint32_t cortex_emu_read32(cortex_emu_t *emu, uint32_t addr);
void     cortex_emu_write8(cortex_emu_t *emu, uint32_t addr, uint32_t val);
void     cortex_emu_write16(cortex_emu_t *emu, uint32_t addr, uint32_t val);
void     cortex_emu_write32(cortex_emu_t *emu, uint32_t addr, uint32_t val);

/* ═══════════════════════════════════════════════════════════════════════
 *  GPIO
 * ═══════════════════════════════════════════════════════════════════════ */

void     cortex_emu_set_gpio_input(cortex_emu_t *emu, uint32_t port, uint32_t value);
uint32_t cortex_emu_get_gpio_input(const cortex_emu_t *emu, uint32_t port);
uint32_t cortex_emu_get_gpio_output(const cortex_emu_t *emu, uint32_t port);
uint32_t cortex_emu_get_gpio_mode(const cortex_emu_t *emu, uint32_t port);
uint32_t cortex_emu_get_gpio_crl(const cortex_emu_t *emu, uint32_t port);
uint32_t cortex_emu_get_gpio_crh(const cortex_emu_t *emu, uint32_t port);

/* ═══════════════════════════════════════════════════════════════════════
 *  ADC
 * ═══════════════════════════════════════════════════════════════════════ */

void cortex_emu_set_adc_value(cortex_emu_t *emu, uint32_t value);
void cortex_emu_set_adc_channel_value(cortex_emu_t *emu, uint32_t channel, uint32_t value);

/* ═══════════════════════════════════════════════════════════════════════
 *  UART
 * ═══════════════════════════════════════════════════════════════════════ */

/** Inject a received byte into UART RDR (external input). */
void cortex_emu_uart_receive(cortex_emu_t *emu, uint32_t uart_idx, uint32_t data);

/** Read next transmitted byte from UART TX buffer. Returns -1 if empty. */
int32_t cortex_emu_uart_read_tx(cortex_emu_t *emu, uint32_t uart_idx);

/** Get count of bytes pending in UART TX buffer. */
int32_t cortex_emu_uart_tx_pending(const cortex_emu_t *emu, uint32_t uart_idx);

/* ═══════════════════════════════════════════════════════════════════════
 *  SPI
 * ═══════════════════════════════════════════════════════════════════════ */

void     cortex_emu_set_spi_rx(cortex_emu_t *emu, uint32_t data);
uint32_t cortex_emu_get_spi_tx(const cortex_emu_t *emu);

/* ═══════════════════════════════════════════════════════════════════════
 *  I2C
 * ═══════════════════════════════════════════════════════════════════════ */

void     cortex_emu_i2c_receive(cortex_emu_t *emu, uint32_t data);
uint32_t cortex_emu_get_i2c_tx(const cortex_emu_t *emu);

/* ═══════════════════════════════════════════════════════════════════════
 *  Interrupts & External Events
 * ═══════════════════════════════════════════════════════════════════════ */

void cortex_emu_trigger_irq(cortex_emu_t *emu, uint32_t irq_num);
void cortex_emu_trigger_exti(cortex_emu_t *emu, uint32_t line, int rising);

/* ═══════════════════════════════════════════════════════════════════════
 *  Timer Injection
 * ═══════════════════════════════════════════════════════════════════════ */

void cortex_emu_inject_tim3_capture(cortex_emu_t *emu, uint32_t period, uint32_t pulse_width);

/* ═══════════════════════════════════════════════════════════════════════
 *  DMA
 * ═══════════════════════════════════════════════════════════════════════ */

void cortex_emu_dma_step(cortex_emu_t *emu);

/* ═══════════════════════════════════════════════════════════════════════
 *  Debugger: Breakpoints & Watchpoints
 * ═══════════════════════════════════════════════════════════════════════ */

int  cortex_emu_add_breakpoint(cortex_emu_t *emu, uint32_t addr);
int  cortex_emu_remove_breakpoint(cortex_emu_t *emu, uint32_t addr);
void cortex_emu_clear_breakpoints(cortex_emu_t *emu);
void cortex_emu_set_watchpoint(cortex_emu_t *emu, uint32_t addr);
void cortex_emu_clear_watchpoint(cortex_emu_t *emu);

/* ═══════════════════════════════════════════════════════════════════════
 *  Status Queries
 * ═══════════════════════════════════════════════════════════════════════ */

int      cortex_emu_is_halted(const cortex_emu_t *emu);
int      cortex_emu_is_sleeping(const cortex_emu_t *emu);
int      cortex_emu_is_lockup(const cortex_emu_t *emu);
int      cortex_emu_is_breakpoint_hit(const cortex_emu_t *emu);
int      cortex_emu_is_watch_triggered(const cortex_emu_t *emu);
uint32_t cortex_emu_get_last_pc(const cortex_emu_t *emu);
uint64_t cortex_emu_get_total_cycles(const cortex_emu_t *emu);
uint32_t cortex_emu_get_active_exception(const cortex_emu_t *emu);
void     cortex_emu_set_halted(cortex_emu_t *emu, int halted);

/* ═══════════════════════════════════════════════════════════════════════
 *  Diagnostics
 * ═══════════════════════════════════════════════════════════════════════ */

uint32_t cortex_emu_get_fault_count(const cortex_emu_t *emu);
uint32_t cortex_emu_get_fault_code(const cortex_emu_t *emu);
uint32_t cortex_emu_get_reset_count(const cortex_emu_t *emu);
uint32_t cortex_emu_get_systick_fire_count(const cortex_emu_t *emu);
uint32_t cortex_emu_get_exception_entry_count(const cortex_emu_t *emu);
uint32_t cortex_emu_get_exception_exit_count(const cortex_emu_t *emu);

/* PC / register trace (circular buffer, 0 = oldest) */
uint32_t cortex_emu_get_pc_trace(const cortex_emu_t *emu, uint32_t index);
uint32_t cortex_emu_get_reg_trace(const cortex_emu_t *emu, uint32_t entry, uint32_t reg_idx);

/* Shadow stack (ring buffer of PC+SP at PUSH / exception entry) */
uint32_t cortex_emu_get_shadow_stack_size(const cortex_emu_t *emu);
uint32_t cortex_emu_get_shadow_stack_pc(const cortex_emu_t *emu, uint32_t index);
uint32_t cortex_emu_get_shadow_stack_sp(const cortex_emu_t *emu, uint32_t index);

/* GPIO write trace */
void     cortex_emu_gpio_trace_start(cortex_emu_t *emu);
void     cortex_emu_gpio_trace_stop(cortex_emu_t *emu);
uint32_t cortex_emu_gpio_trace_count(const cortex_emu_t *emu);
uint32_t cortex_emu_gpio_trace_get(const cortex_emu_t *emu, uint32_t index, uint32_t field);

/* TIM1 effective (shadow / active) output control registers */
uint32_t cortex_emu_get_tim1_eff_ccmr1(const cortex_emu_t *emu);
uint32_t cortex_emu_get_tim1_eff_ccmr2(const cortex_emu_t *emu);
uint32_t cortex_emu_get_tim1_eff_ccer(const cortex_emu_t *emu);

/* Raw memory buffer access (for external inspection) */
uint32_t       cortex_emu_get_mem_size(void);
uint32_t       cortex_emu_get_flash_offset(void);
uint32_t       cortex_emu_get_sram_offset(void);
const uint8_t *cortex_emu_get_mem_ptr(const cortex_emu_t *emu);
uint32_t       cortex_emu_get_it_state(const cortex_emu_t *emu);
uint32_t       cortex_emu_get_xpsr(const cortex_emu_t *emu);

#ifdef __cplusplus
}
#endif

#endif /* CORTEX_EMU_H */
