#include "cortex_emu_internal.h"

/* ── IWDG ─────────────────────────────────────────────────────────────── */

uint32_t emu_iwdg_read(cortex_emu_t *emu, uint32_t addr) {
    uint32_t reg = addr - 0x40003000u;
    if (reg == 0x00) return emu->iwdg_kr;
    if (reg == 0x04) return emu->iwdg_pr;
    if (reg == 0x08) return emu->iwdg_rlr;
    if (reg == 0x0C) return emu->iwdg_sr;
    if (reg == 0x10) return emu->iwdg_winr;
    return 0;
}

void emu_iwdg_write(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    uint32_t reg = addr - 0x40003000u;
    if (reg == 0x00) {
        emu->iwdg_kr = val & 0xFFFF;
        if (val == 0x5555) {
            emu->iwdg_unlocked = 1;
        } else if (val == 0xAAAA) {
            emu->iwdg_counter = emu->iwdg_rlr;
            emu->iwdg_unlocked = 0;
        } else if (val == 0xCCCC) {
            emu->iwdg_running = 1;
            emu->iwdg_counter = emu->iwdg_rlr;
            emu->iwdg_unlocked = 0;
        }
        return;
    }
    if (reg == 0x04 && emu->iwdg_unlocked) { emu->iwdg_pr  = val & 0x07; return; }
    if (reg == 0x08 && emu->iwdg_unlocked) { emu->iwdg_rlr = val & 0x0FFF; return; }
    if (reg == 0x10 && emu->iwdg_unlocked) { emu->iwdg_winr = val & 0x0FFF; return; }
}

void emu_iwdg_reset(cortex_emu_t *emu) {
    emu->iwdg_kr = 0; emu->iwdg_pr = 0; emu->iwdg_rlr = 0x0FFF;
    emu->iwdg_sr = 0; emu->iwdg_winr = 0x0FFF;
    emu->iwdg_counter = 0x0FFF;
    emu->iwdg_running = 0;
    emu->iwdg_unlocked = 0;
}

/* ── WWDG ─────────────────────────────────────────────────────────────── */

uint32_t emu_wwdg_read(cortex_emu_t *emu, uint32_t addr) {
    uint32_t reg = addr - 0x40002C00u;
    if (reg == 0x00) return emu->wwdg_cr;
    if (reg == 0x04) return emu->wwdg_cfr;
    if (reg == 0x08) return emu->wwdg_sr;
    return 0;
}

void emu_wwdg_write(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    uint32_t reg = addr - 0x40002C00u;
    if (reg == 0x00) {
        emu->wwdg_cr = (emu->wwdg_cr & 0x80) | (val & 0xFF);
        if (val & 0x80) emu->wwdg_cr |= 0x80;  /* WDGA sticky */
        return;
    }
    if (reg == 0x04) { emu->wwdg_cfr = val & 0x03FFu; return; }
    if (reg == 0x08) { emu->wwdg_sr &= ~(val & 1); return; }  /* rc_w0 EWIF */
}

void emu_wwdg_reset(cortex_emu_t *emu) {
    emu->wwdg_cr = 0x7F; emu->wwdg_cfr = 0x7F; emu->wwdg_sr = 0;
}

/* ── EXTI ─────────────────────────────────────────────────────────────── */

uint32_t emu_exti_read(cortex_emu_t *emu, uint32_t addr) {
    uint32_t reg = addr - 0x40010400u;
    if (reg == 0x00) return emu->exti_imr;
    if (reg == 0x04) return emu->exti_emr;
    if (reg == 0x08) return emu->exti_rtsr;
    if (reg == 0x0C) return emu->exti_ftsr;
    if (reg == 0x10) return emu->exti_swier;
    if (reg == 0x14) return emu->exti_pr;
    return 0;
}

void emu_exti_write(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    uint32_t reg = addr - 0x40010400u;
    if (reg == 0x00) { emu->exti_imr  = val; return; }
    if (reg == 0x04) { emu->exti_emr  = val; return; }
    if (reg == 0x08) { emu->exti_rtsr = val; return; }
    if (reg == 0x0C) { emu->exti_ftsr = val; return; }
    if (reg == 0x10) {
        emu->exti_swier = val;
        uint32_t triggered = val & emu->exti_imr;
        emu->exti_pr |= triggered;
        if (triggered & 0x03u)   emu->nvic_pending |= (1u << 5);
        if (triggered & 0x0Cu)   emu->nvic_pending |= (1u << 6);
        if (triggered & 0xFFF0u) emu->nvic_pending |= (1u << 7);
        return;
    }
    if (reg == 0x14) { emu->exti_pr &= ~val; return; }  /* rc_w1 */
}

void emu_exti_reset(cortex_emu_t *emu) {
    emu->exti_imr = 0;   emu->exti_emr = 0;
    emu->exti_rtsr = 0;  emu->exti_ftsr = 0;
    emu->exti_swier = 0; emu->exti_pr = 0;
}

/* ── SYSCFG ───────────────────────────────────────────────────────────── */

uint32_t emu_syscfg_read(cortex_emu_t *emu, uint32_t addr) {
    uint32_t reg = addr - 0x40010000u;
    if (reg == 0x00) return emu->syscfg_cfgr1;
    if (reg == 0x08) return emu->syscfg_exticr[0];
    if (reg == 0x0C) return emu->syscfg_exticr[1];
    if (reg == 0x10) return emu->syscfg_exticr[2];
    if (reg == 0x14) return emu->syscfg_exticr[3];
    if (reg == 0x18) return emu->syscfg_cfgr2;
    return 0;
}

void emu_syscfg_write(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    uint32_t reg = addr - 0x40010000u;
    if (reg == 0x00) { emu->syscfg_cfgr1    = val; return; }
    if (reg == 0x08) { emu->syscfg_exticr[0] = val; return; }
    if (reg == 0x0C) { emu->syscfg_exticr[1] = val; return; }
    if (reg == 0x10) { emu->syscfg_exticr[2] = val; return; }
    if (reg == 0x14) { emu->syscfg_exticr[3] = val; return; }
    if (reg == 0x18) { emu->syscfg_cfgr2    = val; return; }
}

void emu_syscfg_reset(cortex_emu_t *emu) {
    emu->syscfg_cfgr1 = 0; emu->syscfg_cfgr2 = 0;
    for (int i = 0; i < 4; i++) emu->syscfg_exticr[i] = 0;
}

/* ── PWR ──────────────────────────────────────────────────────────────── */

uint32_t emu_pwr_read(cortex_emu_t *emu, uint32_t addr) {
    uint32_t reg = addr - 0x40007000u;
    if (reg == 0x00) return emu->pwr_cr;
    if (reg == 0x04) return emu->pwr_csr;
    return 0;
}

void emu_pwr_write(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    uint32_t reg = addr - 0x40007000u;
    if (reg == 0x00) { emu->pwr_cr  = val; return; }
    if (reg == 0x04) { emu->pwr_csr = val; return; }
}

void emu_pwr_reset(cortex_emu_t *emu) {
    emu->pwr_cr = 0; emu->pwr_csr = 0;
}

/* ── CRC ──────────────────────────────────────────────────────────────── */

uint32_t emu_crc_read(cortex_emu_t *emu, uint32_t addr) {
    uint32_t reg = addr - 0x40023000u;
    if (reg == 0x00) return emu->crc_dr;
    if (reg == 0x04) return emu->crc_idr;
    if (reg == 0x08) return emu->crc_cr;
    if (reg == 0x10) return emu->crc_init;
    return 0;
}

void emu_crc_write(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    uint32_t reg = addr - 0x40023000u;
    if (reg == 0x00) {
        uint32_t crc = emu->crc_dr;
        crc ^= val;
        for (int bit = 0; bit < 32; bit++) {
            if (crc & 0x80000000u) crc = (crc << 1) ^ 0x04C11DB7u;
            else                   crc <<= 1;
        }
        emu->crc_dr = crc;
        return;
    }
    if (reg == 0x04) { emu->crc_idr = val & 0xFF; return; }
    if (reg == 0x08) {
        if (val & 1) emu->crc_dr = emu->crc_init;  /* RESET */
        emu->crc_cr = val & ~1u;
        return;
    }
    if (reg == 0x10) { emu->crc_init = val; return; }
}

void emu_crc_reset(cortex_emu_t *emu) {
    emu->crc_dr   = 0xFFFFFFFFu;
    emu->crc_idr  = 0;
    emu->crc_cr   = 0;
    emu->crc_init = 0xFFFFFFFFu;
}
