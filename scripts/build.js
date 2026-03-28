#!/usr/bin/env node
/**
 * build.js — Cross-platform (Windows + Linux + macOS) WASM build for cortex-emu.
 *
 * Toolchain: LLVM  (clang / clang++ / wasm-ld)
 *   Linux/macOS:  apt install clang lld   (or brew install llvm)
 *   Windows:      choco install llvm       (or download from https://llvm.org)
 *
 * Usage:
 *   node scripts/build.js              # build → wasm/dist/cortex_emu.wasm
 *   node scripts/build.js --clean      # clean dist/ then build
 */

const { execFileSync } = require('child_process')
const fs   = require('fs')
const path = require('path')
const os   = require('os')

const ROOT    = path.resolve(__dirname, '..')
const WASM_DIR = path.join(ROOT, 'wasm')
const DIST    = path.join(WASM_DIR, 'dist')
const isWin   = os.platform() === 'win32'
const EXE     = isWin ? '.exe' : ''

/* ── Source files ─────────────────────────────────────────────────────── */
const LIB_SOURCES = [
    'src/cortex_emu.c',
    'src/cpu_thumb16.c',
    'src/cpu_thumb32.c',
    'src/cpu_exceptions.c',
    'src/periph_dispatch.c',
    'src/periph_nvic_systick.c',
    'src/periph_gpio.c',
    'src/periph_rcc.c',
    'src/periph_adc.c',
    'src/periph_flash.c',
    'src/periph_timers.c',
    'src/periph_dma.c',
    'src/periph_uart.c',
    'src/periph_spi_i2c.c',
    'src/periph_misc.c',
].map(f => path.join(ROOT, f))

const GLUE_SOURCE = path.join(WASM_DIR, 'wasm_glue.c')

/* ── Tool discovery ───────────────────────────────────────────────────── */
function findTool(names) {
    for (const name of names) {
        try {
            execFileSync(isWin ? 'where' : 'which', [name + EXE], { stdio: 'pipe' })
            return name + EXE
        } catch { /* not found, try next */ }
    }
    return null
}

const clang  = process.env.CLANG  || findTool(['clang++', 'clang'])
const wasmLd = process.env.WASM_LD || findTool(['wasm-ld', 'wasm-ld-18', 'wasm-ld-17', 'wasm-ld-16'])

if (!clang)  { console.error('error: clang/clang++ not found. Install LLVM.'); process.exit(1) }
if (!wasmLd) { console.error('error: wasm-ld not found. Install LLVM/LLD.');  process.exit(1) }

/* ── Parse args ───────────────────────────────────────────────────────── */
if (process.argv.includes('--clean')) {
    fs.rmSync(DIST, { recursive: true, force: true })
}
fs.mkdirSync(DIST, { recursive: true })

/* ── Compile ──────────────────────────────────────────────────────────── */
const SOURCES = [...LIB_SOURCES, GLUE_SOURCE]
const OBJECTS = []

const CFLAGS = [
    '--target=wasm32-undefined-undefined-wasm',
    '-Wall',
    '-std=c++11',
    '-nostdlib',
    '-O3',
    '-flto',
    '-fvisibility=default',
    '-I' + path.join(WASM_DIR, 'include'),
    '-I' + path.join(ROOT, 'include'),
    '-I' + path.join(ROOT, 'src'),
]

console.log(`Compiling with ${clang} --target=wasm32-undefined-undefined-wasm ...`)
for (const src of SOURCES) {
    const obj = path.join(DIST, path.basename(src, '.c') + '.o')
    execFileSync(clang, [...CFLAGS, '-c', src, '-o', obj], { stdio: 'inherit' })
    OBJECTS.push(obj)
}

/* ── Link ─────────────────────────────────────────────────────────────── */
const WASM_OUT = path.join(DIST, 'cortex_emu.wasm')
const LDFLAGS = [
    '--no-entry',
    '--export-dynamic',
    '--allow-undefined',
    '--lto-O3',
]

console.log(`Linking with ${wasmLd} ...`)
execFileSync(wasmLd, [...LDFLAGS, ...OBJECTS, '-o', WASM_OUT], { stdio: 'inherit' })

/* ── Clean up object files ────────────────────────────────────────────── */
for (const obj of OBJECTS) {
    try { fs.unlinkSync(obj) } catch { /* ignore */ }
}

const stat = fs.statSync(WASM_OUT)
const sizeKB = (stat.size / 1024).toFixed(0)
console.log(`Built: ${WASM_OUT} (${sizeKB} KB)`)
