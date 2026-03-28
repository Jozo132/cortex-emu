#!/usr/bin/env node
/**
 * test.js — Cross-platform test runner for cortex-emu.
 *
 * 1. Builds the WASM module (via scripts/build.js)
 * 2. Instantiates it in Node.js and runs a sanity check
 * 3. Optionally loads mcu_1/flash_dump.bin and runs 50 000 cycles
 *
 * Usage:
 *   node scripts/test.js              # full test (build + WASM check)
 *   node scripts/test.js --skip-build # only run WASM check (assume already built)
 */

const { execFileSync } = require('child_process')
const fs   = require('fs')
const path = require('path')

const ROOT     = path.resolve(__dirname, '..')
const WASM_PATH = path.join(ROOT, 'wasm', 'dist', 'cortex_emu.wasm')
const FW_PATH   = path.join(ROOT, '..', 'mcu_1', 'flash_dump.bin')

let passed = 0
let failed = 0

function assert(cond, msg) {
    if (cond) {
        passed++
        console.log(`  ✓ ${msg}`)
    } else {
        failed++
        console.error(`  ✗ ${msg}`)
    }
}

async function main() {
    /* ── Step 1: Build ─────────────────────────────────────────────────── */
    if (!process.argv.includes('--skip-build')) {
        console.log('Building WASM module ...')
        execFileSync(process.execPath, [path.join(__dirname, 'build.js'), '--clean'], { stdio: 'inherit' })
        console.log('')
    }

    /* ── Step 2: Load WASM module ──────────────────────────────────────── */
    if (!fs.existsSync(WASM_PATH)) {
        console.error(`WASM not found: ${WASM_PATH}`)
        console.error('Run "npm run build" first.')
        process.exit(1)
    }

    console.log('Loading WASM module ...')
    const buf = fs.readFileSync(WASM_PATH)
    const { instance } = await WebAssembly.instantiate(buf, { env: {} })
    const ex = instance.exports
    assert(typeof ex.initialize === 'function', 'WASM exports initialize()')
    assert(typeof ex.cortex_emu_create === 'function', 'WASM exports cortex_emu_create()')

    /* ── Step 3: Sanity test (synthetic flash image) ───────────────────── */
    console.log('\nSanity test (synthetic flash image) ...')
    ex.initialize()
    const h = ex.cortex_emu_create()
    assert(h > 0, 'cortex_emu_create() returns non-null handle')

    /* Configure: M3, 128 KB flash, 8 KB SRAM, 48 MHz */
    ex.cortex_emu_configure(h, 2, 0x08000000, 128 * 1024, 0x20000000, 8 * 1024, 48000000)

    /* Flash: SP=0x20002000, PC=0x08000009, NOP NOP, B . B . */
    ex.cortex_emu_load_flash(h, 0x00, 0x20002000)
    ex.cortex_emu_load_flash(h, 0x04, 0x08000009)
    ex.cortex_emu_load_flash(h, 0x08, 0x46C046C0)
    ex.cortex_emu_load_flash(h, 0x0C, 0xE7FEE7FE)

    ex.cortex_emu_reset(h)
    const sp = ex.cortex_emu_get_reg(h, 13) >>> 0
    const pc = ex.cortex_emu_get_reg(h, 15) >>> 0
    assert(sp === 0x20002000, `SP after reset = 0x${sp.toString(16).toUpperCase()} (expected 0x20002000)`)
    assert(pc === 0x08000008, `PC after reset = 0x${pc.toString(16).toUpperCase()} (expected 0x08000008)`)

    for (let i = 0; i < 10; i++) ex.cortex_emu_step(h)
    assert(ex.cortex_emu_get_fault_count(h) === 0, 'No hard faults after 10 steps')
    assert(!ex.cortex_emu_is_lockup(h), 'CPU not in lockup')

    ex.cortex_emu_destroy(h)

    /* ── Step 4: mcu_1 firmware test (if available) ────────────────────── */
    if (fs.existsSync(FW_PATH)) {
        console.log('\nmcu_1 firmware test ...')
        ex.initialize()
        const h2 = ex.cortex_emu_create()
        ex.cortex_emu_configure(h2, 0, 0x08000000, 32768, 0x20000000, 2048, 48000000)

        const fw = fs.readFileSync(FW_PATH)
        const view = new DataView(fw.buffer, fw.byteOffset, fw.byteLength)
        for (let i = 0; i + 3 < fw.length; i += 4)
            ex.cortex_emu_load_flash(h2, i, view.getUint32(i, true))

        ex.cortex_emu_reset(h2)
        const fwSP = ex.cortex_emu_get_reg(h2, 13) >>> 0
        assert(fwSP === 0x20000740, `mcu_1 SP = 0x${fwSP.toString(16).toUpperCase()} (expected 0x20000740)`)

        const cycles = ex.cortex_emu_run(h2, 50000)
        const faults = ex.cortex_emu_get_fault_count(h2)
        assert(cycles === 50000, `Ran ${cycles} cycles (expected 50000)`)
        assert(faults === 0, `Faults = ${faults} (expected 0)`)

        ex.cortex_emu_destroy(h2)
    } else {
        console.log(`\nSkipping mcu_1 test (${FW_PATH} not found)`)
    }

    /* ── Summary ───────────────────────────────────────────────────────── */
    console.log(`\n${passed} passed, ${failed} failed`)
    process.exit(failed > 0 ? 1 : 0)
}

main().catch(err => {
    console.error(err)
    process.exit(1)
})
