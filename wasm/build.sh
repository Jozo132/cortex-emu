#!/usr/bin/env bash
# build.sh — Bare-metal WASM build for cortex-emu (Linux/macOS shortcut)
#
# Toolchain requirements:
#   apt install clang lld   (Debian/Ubuntu)
#   brew install llvm        (macOS)
#
# Usage:
#   ./wasm/build.sh                       # build → wasm/dist/cortex_emu.wasm
#   ./wasm/build.sh --clean               # clean then build
#   WASM_LD=wasm-ld-18 ./wasm/build.sh   # override linker binary
#
# Cross-platform alternative: npm run build  (uses scripts/build.js)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OUT_DIR="$SCRIPT_DIR/dist"

# ── Tool selection ─────────────────────────────────────────────────────────
CLANG="${CLANG:-clang++}"
if [ -z "$WASM_LD" ]; then
    if command -v wasm-ld >/dev/null 2>&1; then
        WASM_LD=wasm-ld
    elif command -v wasm-ld-18 >/dev/null 2>&1; then
        WASM_LD=wasm-ld-18
    elif command -v wasm-ld-17 >/dev/null 2>&1; then
        WASM_LD=wasm-ld-17
    else
        echo "error: wasm-ld not found. Install llvm/lld." >&2
        exit 1
    fi
fi

# ── Parse arguments ────────────────────────────────────────────────────────
if [ "$1" = "--clean" ]; then
    rm -rf "$OUT_DIR"
fi
mkdir -p "$OUT_DIR"

# ── Source file list ───────────────────────────────────────────────────────
SOURCES=(
    "$REPO_DIR/src/cortex_emu.c"
    "$REPO_DIR/src/cpu_thumb16.c"
    "$REPO_DIR/src/cpu_thumb32.c"
    "$REPO_DIR/src/cpu_exceptions.c"
    "$REPO_DIR/src/periph_dispatch.c"
    "$REPO_DIR/src/periph_nvic_systick.c"
    "$REPO_DIR/src/periph_gpio.c"
    "$REPO_DIR/src/periph_rcc.c"
    "$REPO_DIR/src/periph_adc.c"
    "$REPO_DIR/src/periph_flash.c"
    "$REPO_DIR/src/periph_timers.c"
    "$REPO_DIR/src/periph_dma.c"
    "$REPO_DIR/src/periph_uart.c"
    "$REPO_DIR/src/periph_spi_i2c.c"
    "$REPO_DIR/src/periph_misc.c"
    "$SCRIPT_DIR/wasm_glue.c"
)

# ── Compile ────────────────────────────────────────────────────────────────
echo "Compiling with $CLANG --target=wasm32-undefined-undefined-wasm ..."
OBJECTS=()
for src in "${SOURCES[@]}"; do
    obj="$OUT_DIR/$(basename "$src" .c).o"
    "$CLANG" \
        --target=wasm32-undefined-undefined-wasm \
        -x c -Wall -std=c11 -nostdlib -O3 -flto \
        -fvisibility=default \
        -I"$SCRIPT_DIR/include" \
        -I"$REPO_DIR/include" \
        -I"$REPO_DIR/src" \
        -c "$src" -o "$obj"
    OBJECTS+=("$obj")
done

# ── Link ───────────────────────────────────────────────────────────────────
echo "Linking with $WASM_LD ..."
"$WASM_LD" \
    --no-entry \
    --export-dynamic \
    --allow-undefined \
    --lto-O3 \
    "${OBJECTS[@]}" \
    -o "$OUT_DIR/cortex_emu.wasm"

# ── Clean up object files ─────────────────────────────────────────────────
rm -f "${OBJECTS[@]}"

SIZE=$(ls -lh "$OUT_DIR/cortex_emu.wasm" | awk '{print $5}')
echo "Built: $OUT_DIR/cortex_emu.wasm ($SIZE)"
