#include "cortex_emu_internal.h"

uint32_t emu_dma_read(cortex_emu_t *emu, uint32_t addr) {
    uint32_t reg = addr - 0x40020000u;
    if (reg == 0x00) return emu->dma_isr;
    if (reg >= 0x08 && reg < 0x08 + (uint32_t)(DMA_CHANNELS) * 0x14) {
        uint32_t ch_reg = reg - 0x08;
        uint32_t ch  = ch_reg / 0x14;
        uint32_t off = ch_reg % 0x14;
        if (ch < (uint32_t)DMA_CHANNELS) {
            if (off == 0x00) return emu->dma_ccr[ch];
            if (off == 0x04) return emu->dma_cndtr[ch];
            if (off == 0x08) return emu->dma_cpar[ch];
            if (off == 0x0C) return emu->dma_cmar[ch];
        }
    }
    return 0;
}

void emu_dma_write(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    uint32_t reg = addr - 0x40020000u;
    if (reg == 0x04) { emu->dma_isr &= ~val; return; }
    if (reg >= 0x08 && reg < 0x08 + (uint32_t)(DMA_CHANNELS) * 0x14) {
        uint32_t ch_reg = reg - 0x08;
        uint32_t ch  = ch_reg / 0x14;
        uint32_t off = ch_reg % 0x14;
        if (ch < DMA_CHANNELS) {
            if (off == 0x00) { emu->dma_ccr[ch]   = val; return; }
            if (off == 0x04) { emu->dma_cndtr[ch] = val; return; }
            if (off == 0x08) { emu->dma_cpar[ch]  = val; return; }
            if (off == 0x0C) { emu->dma_cmar[ch]  = val; return; }
        }
    }
}

void emu_dma_tick(cortex_emu_t *emu) {
    for (int ch = 0; ch < DMA_CHANNELS; ch++) {
        uint32_t ccr = emu->dma_ccr[ch];
        if (!(ccr & 1)) continue;
        uint32_t cnt = emu->dma_cndtr[ch];
        if (cnt == 0) continue;
        uint32_t periph = emu->dma_cpar[ch];
        uint32_t mem    = emu->dma_cmar[ch];
        uint32_t dir    = (ccr >> 4) & 1;
        uint32_t msize  = (ccr >> 10) & 3;
        uint32_t psize  = (ccr >> 8) & 3;
        uint32_t minc   = (ccr >> 7) & 1;
        uint32_t pinc   = (ccr >> 6) & 1;
        if (dir == 0) {
            cortex_emu_write32(emu, mem, cortex_emu_read32(emu, periph));
        } else {
            cortex_emu_write32(emu, periph, cortex_emu_read32(emu, mem));
        }
        cnt--;
        emu->dma_cndtr[ch] = cnt;
        if (minc) emu->dma_cmar[ch] = mem + (1u << msize);
        if (pinc) emu->dma_cpar[ch] = emu->dma_cpar[ch] + (1u << psize);
        if (cnt == 0) {
            emu->dma_isr |= 1u << ((uint32_t)ch * 4 + 1);
            if (ccr & 2) {
                int irq;
                if (ch == 0) irq = 9;
                else if (ch <= 2) irq = 10;
                else irq = 11;
                emu->nvic_pending |= (1u << irq);
            }
        }
    }
}
