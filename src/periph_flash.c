#include "cortex_emu_internal.h"

uint32_t emu_flash_read(cortex_emu_t *emu, uint32_t addr) {
    if ((addr & 0x3FF) == 0x00) return emu->flash_acr;
    return 0;
}

void emu_flash_write(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    if ((addr & 0x3FF) == 0x00) emu->flash_acr = val;
}
