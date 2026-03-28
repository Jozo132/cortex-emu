#include "cortex_emu_internal.h"

/* ── NVIC ─────────────────────────────────────────────────────────────── */

uint32_t emu_nvic_read(cortex_emu_t *emu, uint32_t addr) {
    uint32_t reg = addr & 0xFFF;
    if (reg == 0x100) return emu->nvic_enabled;
    if (reg == 0x180) return emu->nvic_enabled;
    if (reg == 0x200) return emu->nvic_pending;
    if (reg == 0x280) return emu->nvic_pending;
    if (reg >= 0x300 && reg < 0x320) return emu->nvic_iabr;
    if (reg >= 0x400 && reg < 0x420) {
        uint32_t idx = (reg - 0x400) >> 2;
        if (idx < 8) return emu->nvic_ipr[idx];
    }
    return 0;
}

void emu_nvic_write(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    uint32_t reg = addr & 0xFFF;
    if (reg == 0x100) { emu->nvic_enabled |= val; return; }
    if (reg == 0x180) { emu->nvic_enabled &= ~val; return; }
    if (reg == 0x200) { emu->nvic_pending |= val; return; }
    if (reg == 0x280) { emu->nvic_pending &= ~val; return; }
    if (reg >= 0x400 && reg < 0x420) {
        uint32_t idx = (reg - 0x400) >> 2;
        if (idx < 8) emu->nvic_ipr[idx] = val;
    }
}

/* ── SysTick ──────────────────────────────────────────────────────────── */

uint32_t emu_systick_read(cortex_emu_t *emu, uint32_t addr) {
    uint32_t reg = addr - 0xE000E010u;
    if (reg == 0x00) return emu->systick_ctrl;
    if (reg == 0x04) return emu->systick_load;
    if (reg == 0x08) {
        uint32_t v = emu->systick_val;
        emu->systick_ctrl &= ~(1u << 16);
        return v;
    }
    if (reg == 0x0C) return emu->systick_calib;
    return 0;
}

void emu_systick_write(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    uint32_t reg = addr - 0xE000E010u;
    if (reg == 0x00) { emu->systick_ctrl = val & 0x00010007u; return; }
    if (reg == 0x04) { emu->systick_load = val & 0x00FFFFFFu; return; }
    if (reg == 0x08) { emu->systick_val = 0; return; }
}

void emu_systick_tick(cortex_emu_t *emu, uint32_t n) {
    if (!(emu->systick_ctrl & 1)) return;
    while (n > 0) {
        if (emu->systick_val == 0) {
            emu->systick_val = emu->systick_load;
            emu->systick_ctrl |= (1u << 16);
            if (emu->systick_ctrl & 2) {
                emu_set_pending_exception(emu, 15);
                emu->systick_fire_count++;
            }
            n--;
        } else if (emu->systick_val >= n) {
            emu->systick_val -= n;
            return;
        } else {
            n -= emu->systick_val;
            emu->systick_val = 0;
        }
    }
}

/* ── SCB ──────────────────────────────────────────────────────────────── */

uint32_t emu_scb_read(cortex_emu_t *emu, uint32_t addr) {
    uint32_t reg = addr - 0xE000ED00u;
    if (reg == 0x00) {
        if (emu->core_type == CORTEX_EMU_CORE_M0)     return 0x410CC200u;
        if (emu->core_type == CORTEX_EMU_CORE_M0PLUS) return 0x410CC601u;
        if (emu->core_type == CORTEX_EMU_CORE_M3)     return 0x412FC230u;
        if (emu->core_type == CORTEX_EMU_CORE_M4)     return 0x410FC241u;
    }
    if (reg == 0x1C) return emu->scb_shpr2;
    if (reg == 0x20) return emu->scb_shpr3;
    return 0;
}

void emu_scb_write(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    uint32_t reg = addr - 0xE000ED00u;
    if (reg == 0x0C && (val & 0xFFFF0000u) == 0x05FA0000u && (val & 4)) {
        /* AIRCR SYSRESETREQ — trigger software reset */
        extern void cortex_emu_reset(cortex_emu_t *);
        cortex_emu_reset(emu);
        return;
    }
    if (reg == 0x1C) { emu->scb_shpr2 = val; return; }
    if (reg == 0x20) { emu->scb_shpr3 = val; return; }
}
