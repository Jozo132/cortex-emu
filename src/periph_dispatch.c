#include "cortex_emu_internal.h"

uint32_t emu_periph_read(cortex_emu_t *emu, uint32_t addr) {
    if (addr >= 0xE000E100u && addr < 0xE000E500u) return emu_nvic_read(emu, addr);
    if (addr >= 0xE000E010u && addr < 0xE000E020u) return emu_systick_read(emu, addr);
    if (addr >= 0xE000ED00u && addr < 0xE000ED40u) return emu_scb_read(emu, addr);
    if (addr >= 0x48000000u && addr < 0x48002000u) return emu_gpio_read(emu, addr);
    if (addr >= 0x40010800u && addr < 0x40011400u) return emu_gpio_read(emu, addr);
    if (addr >= 0x40020000u && addr < 0x40020400u) return emu_dma_read(emu, addr);
    if (addr >= 0x40021000u && addr < 0x40021400u) return emu_rcc_read(emu, addr);
    if (addr >= 0x40012400u && addr < 0x40012800u) return emu_adc_read(emu, addr);
    if (addr >= 0x40022000u && addr < 0x40022400u) return emu_flash_read(emu, addr);
    if (emu_uart_index(addr) >= 0) return emu_uart_read(emu, addr);
    if (addr >= 0x40013000u && addr < 0x40013400u) return emu_spi_read(emu, addr);
    if (addr >= 0x40005400u && addr < 0x40005800u) return emu_i2c_read(emu, addr);
    if (addr >= 0x40003000u && addr < 0x40003400u) return emu_iwdg_read(emu, addr);
    if (addr >= 0x40002C00u && addr < 0x40003000u) return emu_wwdg_read(emu, addr);
    if (addr >= 0x40010400u && addr < 0x40010800u) return emu_exti_read(emu, addr);
    if (addr >= 0x40010000u && addr < 0x40010400u) return emu_syscfg_read(emu, addr);
    if (addr >= 0x40007000u && addr < 0x40007400u) return emu_pwr_read(emu, addr);
    if (addr >= 0x40023000u && addr < 0x40023400u) return emu_crc_read(emu, addr);
    if (addr == 0x40013400u) return 0xCC4350D1u;
    if (addr == 0x1FFFF7E0u) return 0x0040u;
    if (addr == 0xE0042000u) return 0x20036410u;
    if (addr == 0x40015800u) return 0u;
    if (addr == 0x40015804u) return 0u;
    if (emu_timer_index(addr) >= 0) return emu_timer_read(emu, addr);
    return PERIPH_NOT_HANDLED;
}

int emu_periph_write(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    if (addr >= 0xE000E100u && addr < 0xE000E500u) { emu_nvic_write(emu, addr, val); return 1; }
    if (addr >= 0xE000E010u && addr < 0xE000E020u) { emu_systick_write(emu, addr, val); return 1; }
    if (addr >= 0xE000ED00u && addr < 0xE000ED40u) { emu_scb_write(emu, addr, val); return 1; }
    if (addr >= 0x48000000u && addr < 0x48002000u) { emu_gpio_write(emu, addr, val); return 1; }
    if (addr >= 0x40010800u && addr < 0x40011400u) { emu_gpio_write(emu, addr, val); return 1; }
    if (addr >= 0x40020000u && addr < 0x40020400u) { emu_dma_write(emu, addr, val); return 1; }
    if (addr >= 0x40021000u && addr < 0x40021400u) { emu_rcc_write(emu, addr, val); return 1; }
    if (addr >= 0x40012400u && addr < 0x40012800u) { emu_adc_write(emu, addr, val); return 1; }
    if (addr >= 0x40022000u && addr < 0x40022400u) { emu_flash_write(emu, addr, val); return 1; }
    if (emu_uart_index(addr) >= 0) { emu_uart_write(emu, addr, val); return 1; }
    if (addr >= 0x40013000u && addr < 0x40013400u) { emu_spi_write(emu, addr, val); return 1; }
    if (addr >= 0x40005400u && addr < 0x40005800u) { emu_i2c_write(emu, addr, val); return 1; }
    if (addr >= 0x40003000u && addr < 0x40003400u) { emu_iwdg_write(emu, addr, val); return 1; }
    if (addr >= 0x40002C00u && addr < 0x40003000u) { emu_wwdg_write(emu, addr, val); return 1; }
    if (addr >= 0x40010400u && addr < 0x40010800u) { emu_exti_write(emu, addr, val); return 1; }
    if (addr >= 0x40010000u && addr < 0x40010400u) { emu_syscfg_write(emu, addr, val); return 1; }
    if (addr >= 0x40007000u && addr < 0x40007400u) { emu_pwr_write(emu, addr, val); return 1; }
    if (addr >= 0x40023000u && addr < 0x40023400u) { emu_crc_write(emu, addr, val); return 1; }
    if (emu_timer_index(addr) >= 0) { emu_timer_write(emu, addr, val); return 1; }
    return 0;
}
