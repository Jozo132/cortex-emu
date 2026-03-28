cmake_minimum_required(VERSION 3.10)
project(cortex_emu C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SOURCES
    src/cortex_emu.c
    src/cpu_thumb16.c
    src/cpu_thumb32.c
    src/cpu_exceptions.c
    src/periph_dispatch.c
    src/periph_nvic_systick.c
    src/periph_gpio.c
    src/periph_rcc.c
    src/periph_adc.c
    src/periph_flash.c
    src/periph_timers.c
    src/periph_dma.c
    src/periph_uart.c
    src/periph_spi_i2c.c
    src/periph_misc.c
)

add_library(cortex_emu STATIC ${SOURCES})
target_include_directories(cortex_emu PUBLIC include PRIVATE src)
target_compile_options(cortex_emu PRIVATE -Wall -Wextra -Wno-unused-parameter)

add_executable(cortex_emu_test tests/test_basic.c)
target_link_libraries(cortex_emu_test cortex_emu)
target_include_directories(cortex_emu_test PRIVATE include src)
target_compile_options(cortex_emu_test PRIVATE -Wall -Wextra -Wno-unused-parameter)

# ── WebAssembly build ──────────────────────────────────────────────────────
# The WASM build uses a bare-metal LLVM pipeline (clang++ + wasm-ld, not Emscripten).
# Cross-platform:  npm run build       → wasm/dist/cortex_emu.wasm
# Linux/macOS:     bash wasm/build.sh  → same output
#
# Compile: clang++ --target=wasm32-undefined-undefined-wasm -std=c++11 -nostdlib -O3 -flto
# Link:    wasm-ld --no-entry --export-dynamic --allow-undefined --lto-O3
