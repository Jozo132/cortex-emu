# cortex-emu — Portable ARM Cortex-M Emulator (C/C++ / WASM)

A standalone, dependency-free C99 library that emulates ARM Cortex-M0, M0+, M3, and M4 microcontrollers. Compiled either as a native static library or (via Emscripten) as a WebAssembly module.

## Features

- **CPU cores**: Cortex-M0, M0+, M3, M4 (full ARMv6-M / ARMv7-M Thumb + Thumb-2 ISA)
- **Interrupts**: NVIC with priority levels and preemption, SysTick, exception entry/exit
- **Peripherals**: GPIO (MM32F0020 + STM32F0/F1 layouts), TIM1/2/3/4/14/15, DMA (5 ch), UART ×2, SPI, I2C, ADC, RCC, IWDG, WWDG, EXTI, SYSCFG, CRC-32, PWR
- **Debugger**: breakpoints, watchpoints, PC/register trace, shadow stack
- **WASM-ready**: clean C99, no POSIX dependencies beyond `<stdlib.h>`/`<string.h>`

## Directory layout

```
cortex-emu/
├── include/
│   └── cortex_emu.h          ← Public API (single header)
├── src/
│   ├── cortex_emu_internal.h ← Internal struct + helpers (not public)
│   ├── cortex_emu.c          ← Lifecycle, memory, step/run, registers
│   ├── cpu_thumb16.c         ← ARMv6-M Thumb-16 instruction decoder
│   ├── cpu_thumb32.c         ← ARMv7-M Thumb-2 (32-bit) instruction decoder
│   ├── cpu_exceptions.c      ← Exception entry/exit, NVIC priority, condition eval
│   ├── periph_dispatch.c     ← Peripheral address router (read/write)
│   ├── periph_nvic_systick.c ← NVIC + SysTick + SCB
│   ├── periph_gpio.c         ← GPIO (MM32/STM32F0 + STM32F1 dual layouts)
│   ├── periph_rcc.c          ← RCC with auto-ready flags
│   ├── periph_adc.c          ← ADC (HK32/MM32 + STM32F103 layouts)
│   ├── periph_flash.c        ← Flash interface ACR stub
│   ├── periph_timers.c       ← TIM1/2/3/4/14/15 with edge/center modes
│   ├── periph_dma.c          ← 5-channel DMA controller
│   ├── periph_uart.c         ← UART ×2 with TX ring buffer
│   ├── periph_spi_i2c.c      ← SPI1 + I2C1
│   └── periph_misc.c         ← IWDG, WWDG, EXTI, SYSCFG, PWR, CRC-32
├── tests/
│   └── test_basic.c          ← Sanity test (configure + load + run)
└── CMakeLists.txt            ← CMake build (static lib + test executable)
```

## Building (native)

```bash
mkdir build && cd build
cmake ..
make
./cortex_emu_test      # → PASS
```

Or compile directly with GCC:

```bash
gcc -std=c99 -O2 -Wall -Iinclude -Isrc \
    src/cortex_emu.c src/cpu_thumb16.c src/cpu_thumb32.c src/cpu_exceptions.c \
    src/periph_dispatch.c src/periph_nvic_systick.c src/periph_gpio.c \
    src/periph_rcc.c src/periph_adc.c src/periph_flash.c \
    src/periph_timers.c src/periph_dma.c src/periph_uart.c \
    src/periph_spi_i2c.c src/periph_misc.c \
    tests/test_basic.c -o cortex_emu_test
```

## Building (WebAssembly — bare-metal LLVM)

The WASM target uses a self-contained LLVM toolchain (**no Emscripten**).  
Required packages: `clang` + `lld` (ships with LLVM ≥ 16).

```bash
# Cross-platform (Windows + Linux + macOS) — recommended
cd cortex-emu
npm run build                 # → wasm/dist/cortex_emu.wasm (~58 KB)
npm test                      # build + run WASM sanity tests

# Linux/macOS shortcut (bash)
bash wasm/build.sh            # same output
```

Compile command: `clang++ --target=wasm32-undefined-undefined-wasm -std=c++11 -nostdlib -O3 -flto`  
Link command: `wasm-ld --no-entry --export-dynamic --allow-undefined --lto-O3`

No OS, no libc, no Emscripten runtime — the resulting `.wasm` loads in any
WebAssembly host.

### JS interface (hand-written)

`wasm/cortex_emu.js` is a hand-written ES-module wrapper that works in both
**Node.js** and the **browser** (no bundler required):

```js
import CortexEmu from './wasm/cortex_emu.js'

const lib = await new CortexEmu('./wasm/dist/cortex_emu.wasm').initialize()
const h   = lib.create()
lib.configure(h, CortexEmu.CORE_M0,
              0x08000000, 32768,   // flash
              0x20000000, 2048,    // SRAM
              48000000)            // 48 MHz
lib.loadFirmware(h, firmwareBytes) // Uint8Array / Buffer
lib.reset(h)
lib.run(h, 100000)
console.log('PC =', lib.getReg(h, 15).toString(16))
lib.destroy(h)
```

The JS wrapper exposes the full `cortex_emu.h` API as methods of the `CortexEmu`
class.  WASM linear memory is accessible via `lib.memory` (`Uint8Array`) and
`lib.view` (`DataView`) for direct inspection.

## Quick start

```c
#include "cortex_emu.h"

cortex_emu_t *emu = cortex_emu_create();

// Configure for MM32F0020 (M0, 32 KB flash, 2 KB SRAM, 48 MHz)
cortex_emu_configure(emu, CORTEX_EMU_CORE_M0,
                     0x08000000, 32768,
                     0x20000000, 2048,
                     48000000);

// Load firmware (word by word)
for (uint32_t i = 0; i + 3 < fw_size; i += 4)
    cortex_emu_load_flash(emu, i, *(uint32_t *)(fw_bytes + i));

// Reset (loads SP/PC from vector table)
cortex_emu_reset(emu);

// Run 100 000 cycles
cortex_emu_run(emu, 100000);

// Inspect state
printf("PC=0x%08X  faults=%u\n",
       cortex_emu_get_reg(emu, 15),
       cortex_emu_get_fault_count(emu));

// Set GPIO input (e.g. hall sensor)
cortex_emu_set_gpio_input(emu, 0 /*GPIOA*/, 1 << 3);

// Inject an ADC value
cortex_emu_set_adc_channel_value(emu, 1, 2048);

cortex_emu_destroy(emu);
```

## API overview

See `include/cortex_emu.h` for the full documented API. Key functions:

| Function | Description |
|---|---|
| `cortex_emu_create()` | Allocate emulator instance |
| `cortex_emu_configure(emu, core, ...)` | Set core type, memory map, clock |
| `cortex_emu_load_flash(emu, offset, word)` | Write 32-bit word to flash region |
| `cortex_emu_reset(emu)` | Reset CPU, load SP/PC from vector table |
| `cortex_emu_step(emu)` | Execute one instruction, return cycle count |
| `cortex_emu_run(emu, max_cycles)` | Execute up to N cycles |
| `cortex_emu_get_reg(emu, idx)` | Read register (0-15=R0-R15, 16=xPSR, ...) |
| `cortex_emu_set_gpio_input(emu, port, val)` | Drive GPIO input pins |
| `cortex_emu_get_gpio_output(emu, port)` | Read GPIO output state |
| `cortex_emu_set_adc_channel_value(emu, ch, val)` | Set per-channel ADC value |
| `cortex_emu_add_breakpoint(emu, addr)` | Set a code breakpoint |
| `cortex_emu_get_fault_count(emu)` | Number of hard faults |

## License

MIT
