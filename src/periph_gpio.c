#include "cortex_emu_internal.h"

/* GPIO port index: 0=A, 1=B, 2=C(F1 only), 3=F */
static int gpio_port_index(uint32_t addr) {
    /* STM32F1: GPIOA=0x40010800, GPIOB=0x40010C00, GPIOC=0x40011000 */
    if (addr >= 0x40010800u && addr < 0x40010C00u) return 0;
    if (addr >= 0x40010C00u && addr < 0x40011000u) return 1;
    if (addr >= 0x40011000u && addr < 0x40011400u) return 2;
    /* 0x48000xxx Cortex-M0 layout */
    if (addr >= 0x48000000u && addr < 0x48000400u) return 0;
    if (addr >= 0x48000400u && addr < 0x48000800u) return 1;
    if (addr >= 0x48001400u && addr < 0x48001800u) return 3;
    return -1;
}

static int is_f1_gpio(uint32_t addr) {
    return addr >= 0x40010800u && addr < 0x40011400u;
}

uint32_t emu_gpio_read(cortex_emu_t *emu, uint32_t addr) {
    int port = gpio_port_index(addr);
    if (port < 0) return 0;
    uint32_t reg = addr & 0x3FF;
    if (is_f1_gpio(addr)) {
        if (reg == 0x00) return emu->gpio_crl[port];
        if (reg == 0x04) return emu->gpio_crh[port];
        if (reg == 0x08) return emu->gpio_idr[port] | emu->gpio_odr[port];
        if (reg == 0x0C) return emu->gpio_odr[port];
        return 0;
    }
    if (reg == 0x00) return emu->gpio_moder[port];
    if (reg == 0x04) return emu->gpio_otyper[port];
    /* +0x08: OSPEEDR (std F0) or IDR (MM32) — return IDR|ODR for both */
    if (reg == 0x08) return emu->gpio_idr[port] | emu->gpio_odr[port];
    /* +0x0C: PUPDR (std F0) or ODR (MM32) */
    if (reg == 0x0C) return emu->gpio_odr[port];
    if (reg == 0x10) return emu->gpio_idr[port] | emu->gpio_odr[port];
    if (reg == 0x14) return emu->gpio_odr[port];
    if (reg == 0x18) return 0;  /* BSRR write-only */
    if (reg == 0x1C) return emu->gpio_lckr[port];
    if (reg == 0x20) return emu->gpio_afrl[port];
    if (reg == 0x24) return emu->gpio_afrh[port];
    return 0;
}

void emu_gpio_write(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    int port = gpio_port_index(addr);
    if (port < 0) return;
    uint32_t reg = addr & 0x3FF;
    if (port == 0 && reg >= 0x08)
        emu_gpio_trace_record(emu, port, reg, val); /* trace GPIOA only (matches reference for commutation debug) */
    if (is_f1_gpio(addr)) {
        if (reg == 0x00) { emu->gpio_crl[port] = val; return; }
        if (reg == 0x04) { emu->gpio_crh[port] = val; return; }
        if (reg == 0x0C) { emu->gpio_odr[port] = val & 0xFFFF; return; }
        if (reg == 0x10) {  /* BSRR */
            uint32_t odr = emu->gpio_odr[port];
            odr |= val & 0xFFFF;
            odr &= ~((val >> 16) & 0xFFFF);
            emu->gpio_odr[port] = odr;
            return;
        }
        if (reg == 0x14) { emu->gpio_odr[port] &= ~(val & 0xFFFF); return; } /* BRR */
        return;
    }
    if (reg == 0x00) { emu->gpio_moder[port] = val; return; }
    if (reg == 0x04) { emu->gpio_otyper[port] = val; return; }
    if (reg == 0x08) { emu->gpio_ospeedr[port] = val; return; }  /* OSPEEDR / MM32 IDR read-only */
    if (reg == 0x0C) {
        /* PUPDR (std F0) / ODR (MM32) */
        emu->gpio_odr[port] = val & 0xFFFF;
        emu->gpio_pupdr[port] = val;
        return;
    }
    if (reg == 0x10) {
        /* IDR (std F0, ro) / BRR (MM32) — treat as bit-reset */
        emu->gpio_odr[port] &= ~(val & 0xFFFF);
        return;
    }
    if (reg == 0x14) {
        /* ODR (std F0) / BSR (MM32) — bit-set for MM32 compat */
        emu->gpio_odr[port] |= val & 0xFFFF;
        return;
    }
    if (reg == 0x18) {  /* BSRR */
        uint32_t odr = emu->gpio_odr[port];
        odr |= val & 0xFFFF;
        odr &= ~((val >> 16) & 0xFFFF);
        emu->gpio_odr[port] = odr;
        return;
    }
    if (reg == 0x1C) { emu->gpio_lckr[port] = val; return; }
    if (reg == 0x20) { emu->gpio_afrl[port] = val; return; }
    if (reg == 0x24) { emu->gpio_afrh[port] = val; return; }
    if (reg == 0x28) {  /* BRR (std STM32F0 separate register) */
        emu->gpio_odr[port] &= ~(val & 0xFFFF);
        return;
    }
}
