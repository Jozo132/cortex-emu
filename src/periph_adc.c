#include "cortex_emu_internal.h"

static int adc_select_hk32_channel(const cortex_emu_t *emu) {
    uint32_t mask = emu->adc_chselr;
    for (int ch = 0; ch < ADC_CHANNELS; ch++) {
        if (mask & (1u << ch)) return ch;
    }
    return -1;
}

uint32_t emu_adc_read(cortex_emu_t *emu, uint32_t addr) {
    uint32_t reg = addr - 0x40012400u;
    if (reg == 0x00) return emu->adc_isr;
    if (reg == 0x04) return emu->adc_ier;
    if (reg == 0x08) return emu->adc_cr;
    if (reg == 0x0C) return emu->adc_cfgr1;
    if (reg == 0x10) return emu->adc_smpr;  /* F103 SMPR2 */
    if (reg == 0x14) return emu->adc_smpr;  /* HK32 SMPR */
    if (reg == 0x28) return emu->adc_chselr;
    if (reg == 0x2C) return emu->adc_sqr1;
    if (reg == 0x34) return emu->adc_sqr3;
    if (reg == 0x40) return emu->adc_dr;    /* HK32 DR */
    if (reg == 0x4C) {                      /* F103 DR — clear EOC on read */
        emu->adc_isr &= ~(1u << 1);
        return emu->adc_dr;
    }
    return 0;
}

void emu_adc_write(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    uint32_t reg = addr - 0x40012400u;
    if (reg == 0x00) { emu->adc_isr &= ~val; return; }  /* rc_w1 */
    if (reg == 0x04) { emu->adc_ier = val; return; }
    if (reg == 0x08) {
        emu->adc_cr = val;
        if (emu->adc_cr & (1u << 31)) emu->adc_cr &= ~(1u << 31);  /* ADCAL done */
        if (emu->adc_cr & 1u) emu->adc_isr |= 1u;                   /* ADEN → ADRDY */
        if (emu->adc_cr & 4u) {
            int hk32_ch = adc_select_hk32_channel(emu);
            if (hk32_ch >= 0) emu->adc_dr = emu->adc_channel_values[hk32_ch];
            emu->adc_isr |= 4u;   /* ADSTART → EOC */
            emu->adc_cr  &= ~4u;  /* self-clearing */
        }
        if (emu->adc_cr & 8u) emu->adc_cr &= ~8u;  /* RSTCAL done */
        if (emu->adc_cr & 1u) {
            emu->adc_isr |= (1u << 1);  /* EOC */
            uint32_t ch = emu->adc_sqr3 & 0x1Fu;
            if (ch < ADC_CHANNELS) emu->adc_dr = emu->adc_channel_values[ch];
        }
        return;
    }
    if (reg == 0x0C) { emu->adc_cfgr1 = val; return; }
    if (reg == 0x10) { emu->adc_smpr = val; return; }
    if (reg == 0x14) { emu->adc_smpr = val; return; }
    if (reg == 0x28) { emu->adc_chselr = val; return; }
    if (reg == 0x2C) { emu->adc_sqr1 = val; return; }
    if (reg == 0x34) { emu->adc_sqr3 = val; return; }
}
