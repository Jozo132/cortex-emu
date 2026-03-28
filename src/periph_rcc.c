#include "cortex_emu_internal.h"

uint32_t emu_rcc_read(cortex_emu_t *emu, uint32_t addr) {
    uint32_t reg = addr - 0x40021000u;
    if (reg == 0x00) return emu->rcc_cr;
    if (reg == 0x04) return emu->rcc_cfgr;
    if (reg == 0x08) return emu->rcc_cir;
    if (reg == 0x0C) return emu->rcc_apb2rstr;
    if (reg == 0x10) return emu->rcc_apb1rstr;
    if (reg == 0x14) return emu->rcc_ahbenr;
    if (reg == 0x18) return emu->rcc_apb2enr;
    if (reg == 0x1C) return emu->rcc_apb1enr;
    return 0;
}

void emu_rcc_write(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    uint32_t reg = addr - 0x40021000u;
    if (reg == 0x00) {
        emu->rcc_cr = val;
        if (emu->rcc_cr & 0x01u) emu->rcc_cr |= 0x02u;          /* HSION → HSIRDY */
        if (emu->rcc_cr & (1u << 16)) emu->rcc_cr |= (1u << 17); /* HSEON → HSERDY */
        if (emu->rcc_cr & (1u << 24)) emu->rcc_cr |= (1u << 25); /* PLLON → PLLRDY */
        return;
    }
    if (reg == 0x04) {
        emu->rcc_cfgr = val;
        uint32_t sw = emu->rcc_cfgr & 3;
        emu->rcc_cfgr = (emu->rcc_cfgr & ~0x0Cu) | (sw << 2);  /* SWS mirrors SW */
        return;
    }
    if (reg == 0x08) { emu->rcc_cir = 0; return; }
    if (reg == 0x0C) { emu->rcc_apb2rstr = val; return; }
    if (reg == 0x10) { emu->rcc_apb1rstr = val; return; }
    if (reg == 0x14) { emu->rcc_ahbenr = val; return; }
    if (reg == 0x18) { emu->rcc_apb2enr = val; return; }
    if (reg == 0x1C) { emu->rcc_apb1enr = val; return; }
}
