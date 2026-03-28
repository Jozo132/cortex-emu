#include "cortex_emu_internal.h"

/* ── SPI ──────────────────────────────────────────────────────────────── */

uint32_t emu_spi_read(cortex_emu_t *emu, uint32_t addr) {
    uint32_t reg = addr - 0x40013000u;
    if (reg == 0x00) return emu->spi_cr1;
    if (reg == 0x04) return emu->spi_cr2;
    if (reg == 0x08) return emu->spi_sr;
    if (reg == 0x0C) {
        emu->spi_sr &= ~1u;  /* clear RXNE */
        return emu->spi_rx_data;
    }
    if (reg == 0x10) return emu->spi_crcpr;
    if (reg == 0x14) return emu->spi_rxcrcr;
    if (reg == 0x18) return emu->spi_txcrcr;
    if (reg == 0x1C) return emu->spi_i2scfgr;
    if (reg == 0x20) return emu->spi_i2spr;
    return 0;
}

void emu_spi_write(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    uint32_t reg = addr - 0x40013000u;
    if (reg == 0x00) { emu->spi_cr1 = val; return; }
    if (reg == 0x04) { emu->spi_cr2 = val; return; }
    if (reg == 0x08) { emu->spi_sr  = val; return; }
    if (reg == 0x0C) {
        emu->spi_dr = val;
        emu->spi_sr |= 0x0002u;  /* TXE */
        emu->spi_sr |= 0x0001u;  /* RXNE — loopback in emulation */
        emu->spi_rx_data = val;
        if ((emu->spi_cr2 & (1u << 7)) || (emu->spi_cr2 & (1u << 6)))
            emu->nvic_pending |= (1u << 25);  /* SPI1 = IRQ25 */
        return;
    }
    if (reg == 0x10) { emu->spi_crcpr   = val; return; }
    if (reg == 0x1C) { emu->spi_i2scfgr = val; return; }
    if (reg == 0x20) { emu->spi_i2spr   = val; return; }
}

void emu_spi_reset(cortex_emu_t *emu) {
    emu->spi_cr1 = 0;   emu->spi_cr2 = 0;
    emu->spi_sr  = 0x0002u;  /* TXE=1 */
    emu->spi_dr  = 0;   emu->spi_crcpr  = 0x0007u;
    emu->spi_rxcrcr = 0; emu->spi_txcrcr = 0;
    emu->spi_i2scfgr = 0; emu->spi_i2spr = 0x0002u;
    emu->spi_rx_data = 0;
}

/* ── I2C ──────────────────────────────────────────────────────────────── */

uint32_t emu_i2c_read(cortex_emu_t *emu, uint32_t addr) {
    uint32_t reg = addr - 0x40005400u;
    if (reg == 0x00) return emu->i2c_cr1;
    if (reg == 0x04) return emu->i2c_cr2;
    if (reg == 0x08) return emu->i2c_oar1;
    if (reg == 0x0C) return emu->i2c_oar2;
    if (reg == 0x10) return emu->i2c_timingr;
    if (reg == 0x14) return emu->i2c_timeoutr;
    if (reg == 0x18) return emu->i2c_isr;
    if (reg == 0x1C) return emu->i2c_icr;
    if (reg == 0x20) return emu->i2c_pecr;
    if (reg == 0x24) {
        emu->i2c_isr &= ~(1u << 2);  /* clear RXNE */
        return emu->i2c_rxdr;
    }
    if (reg == 0x28) return emu->i2c_txdr;
    return 0;
}

void emu_i2c_write(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    uint32_t reg = addr - 0x40005400u;
    if (reg == 0x00) {
        emu->i2c_cr1 = val;
        if (!(val & 1)) emu->i2c_isr = 0x00000001u;  /* PE cleared → reset, TXE=1 */
        return;
    }
    if (reg == 0x04) {
        emu->i2c_cr2 = val;
        if (val & (1u << 13)) {  /* START */
            emu->i2c_isr |= (1u << 15);   /* BUSY */
            emu->i2c_isr &= ~(1u << 5);   /* clear STOPF */
            emu->i2c_isr |= (1u << 1);    /* TXIS */
        }
        if (val & (1u << 14)) {  /* STOP */
            emu->i2c_isr &= ~(1u << 15);  /* clear BUSY */
            emu->i2c_isr |= (1u << 5);    /* STOPF */
            if (emu->i2c_cr1 & (1u << 5))
                emu->nvic_pending |= (1u << 23);  /* I2C1 = IRQ23 */
        }
        return;
    }
    if (reg == 0x08) { emu->i2c_oar1 = val; return; }
    if (reg == 0x0C) { emu->i2c_oar2 = val; return; }
    if (reg == 0x10) { emu->i2c_timingr = val; return; }
    if (reg == 0x14) { emu->i2c_timeoutr = val; return; }
    if (reg == 0x1C) { emu->i2c_isr &= ~val; return; }  /* ICR: write-1-to-clear */
    if (reg == 0x28) {
        emu->i2c_txdr = val & 0xFF;
        emu->i2c_isr |= (1u << 0);  /* TXE */
        emu->i2c_isr |= (1u << 1);  /* TXIS */
        if (emu->i2c_cr1 & (1u << 1))
            emu->nvic_pending |= (1u << 23);
        return;
    }
}

void emu_i2c_reset(cortex_emu_t *emu) {
    emu->i2c_cr1 = 0;   emu->i2c_cr2 = 0;
    emu->i2c_oar1 = 0;  emu->i2c_oar2 = 0;
    emu->i2c_timingr = 0; emu->i2c_timeoutr = 0;
    emu->i2c_isr = 0x00000001u;  /* TXE=1 */
    emu->i2c_icr = 0;   emu->i2c_pecr = 0;
    emu->i2c_rxdr = 0;  emu->i2c_txdr = 0;
}
