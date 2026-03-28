/*
 * cortex-emu — Core implementation
 *
 * Create/destroy, configure, reset, step, run, memory access,
 * address translation, bit-band support.
 */

#include "cortex_emu_internal.h"
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════
 *  Lifecycle
 * ═══════════════════════════════════════════════════════════════════════ */

cortex_emu_t *cortex_emu_create(void) {
    cortex_emu_t *emu = (cortex_emu_t *)calloc(1, sizeof(cortex_emu_t));
    return emu;
}

void cortex_emu_destroy(cortex_emu_t *emu) {
    free(emu);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Configuration
 * ═══════════════════════════════════════════════════════════════════════ */

void cortex_emu_configure(cortex_emu_t *emu, uint32_t core,
                           uint32_t flash_base, uint32_t flash_size,
                           uint32_t sram_base, uint32_t sram_size,
                           uint32_t clock_hz) {
    emu->core_type  = core;
    emu->flash_base = flash_base;
    emu->flash_size = flash_size;
    emu->sram_base  = sram_base;
    emu->sram_size  = sram_size;
    uint32_t hz = clock_hz > 0 ? clock_hz : 48000000u;
    emu->systick_calib = hz / 1000;

    /* Initialize peripheral state to power-on defaults */
    emu_uart_reset(emu);
    emu_spi_reset(emu);
    emu_i2c_reset(emu);
    emu_iwdg_reset(emu);
    emu_wwdg_reset(emu);
    emu_exti_reset(emu);
    emu_syscfg_reset(emu);
    emu_pwr_reset(emu);
    emu_crc_reset(emu);

    /* Fill flash region with 0xFF (erased NOR flash state) */
    uint32_t flash_end = CORTEX_EMU_FLASH_OFFSET + flash_size;
    if (flash_end > CORTEX_EMU_MEM_SIZE) flash_end = CORTEX_EMU_MEM_SIZE;
    memset(emu->mem + CORTEX_EMU_FLASH_OFFSET, 0xFF, flash_end - CORTEX_EMU_FLASH_OFFSET);
}

void cortex_emu_load_flash(cortex_emu_t *emu, uint32_t offset, uint32_t value) {
    uint32_t addr = CORTEX_EMU_FLASH_OFFSET + offset;
    if (addr < (uint32_t)CORTEX_EMU_MEM_SIZE - 3)
        mem_store32(emu->mem, addr, value);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Reset
 * ═══════════════════════════════════════════════════════════════════════ */

void cortex_emu_reset(cortex_emu_t *emu) {
    emu->reset_count++;

    for (int i = 0; i < REG_COUNT; i++) emu->regs[i] = 0;
    emu->xpsr = 0; emu->primask = 0; emu->control = 0;
    emu->basepri = 0; emu->faultmask = 0;
    emu->it_state = 0;
    emu->halted = 0; emu->sleeping = 0; emu->lockup = 0;
    emu->active_exception = 0; emu->total_cycles = 0;

    emu->nvic_enabled = 0; emu->nvic_pending = 0; emu->nvic_iabr = 0;
    for (int i = 0; i < 8; i++) emu->nvic_ipr[i] = 0;
    emu->scb_shpr2 = 0; emu->scb_shpr3 = 0;

    emu->systick_ctrl = 0; emu->systick_load = 0; emu->systick_val = 0;
    emu->dma_isr = 0;
    for (int i = 0; i < DMA_CHANNELS; i++) {
        emu->dma_ccr[i] = 0; emu->dma_cndtr[i] = 0;
        emu->dma_cpar[i] = 0; emu->dma_cmar[i] = 0;
    }

    for (int i = 0; i < GPIO_PORTS; i++) {
        emu->gpio_idr[i] = 0; emu->gpio_odr[i] = 0; emu->gpio_moder[i] = 0;
        emu->gpio_crl[i] = 0x44444444u; emu->gpio_crh[i] = 0x44444444u;
        emu->gpio_ospeedr[i] = 0; emu->gpio_pupdr[i] = 0;
        emu->gpio_otyper[i] = 0; emu->gpio_lckr[i] = 0;
        emu->gpio_afrl[i] = 0; emu->gpio_afrh[i] = 0;
    }

    emu->rcc_cr = 0; emu->rcc_cfgr = 0; emu->rcc_cir = 0;
    emu->rcc_apb2rstr = 0; emu->rcc_apb1rstr = 0;
    emu->rcc_ahbenr = 0; emu->rcc_apb2enr = 0; emu->rcc_apb1enr = 0;

    emu->adc_isr = 0; emu->adc_ier = 0; emu->adc_cr = 0;
    emu->adc_cfgr1 = 0; emu->adc_smpr = 0; emu->adc_chselr = 0;
    emu->adc_dr = 512;
    emu->adc_sqr1 = 0; emu->adc_sqr3 = 0;
    for (int i = 0; i < ADC_CHANNELS; i++) emu->adc_channel_values[i] = 512;

    emu->flash_acr = 0;
    emu_timer_reset(emu);
    emu_uart_reset(emu);
    emu_spi_reset(emu);
    emu_i2c_reset(emu);
    emu_iwdg_reset(emu);
    emu_wwdg_reset(emu);
    emu_exti_reset(emu);
    emu_syscfg_reset(emu);
    emu_pwr_reset(emu);
    emu_crc_reset(emu);

    emu->breakpoint_count = 0; emu->breakpoint_hit = 0;
    emu->watch_enabled = 0; emu->watch_triggered = 0;

    memset(emu->pc_trace, 0, sizeof(emu->pc_trace));
    memset(emu->reg_trace_data, 0, sizeof(emu->reg_trace_data));
    emu->pc_trace_idx = 0;

    memset(emu->shadow_stack_pc, 0, sizeof(emu->shadow_stack_pc));
    memset(emu->shadow_stack_sp, 0, sizeof(emu->shadow_stack_sp));
    emu->shadow_stack_idx = 0;
    emu->shadow_stack_count = 0;
    emu->hard_fault_code = 0;

    /* Read initial SP and PC from vector table */
    emu->regs[13] = cortex_emu_read32(emu, emu->flash_base);
    emu->msp = emu->regs[13];
    emu->regs[15] = cortex_emu_read32(emu, emu->flash_base + 4) & ~(uint32_t)1;
    emu->xpsr = 0x01000000u;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Address Translation
 * ═══════════════════════════════════════════════════════════════════════ */

static int32_t translate_periph_addr(cortex_emu_t *emu, uint32_t addr) {
    uint32_t off = addr & 0x0FFFFFFFu;
    (void)emu;
    if (off < 0x400) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_TIM2_OFF + off);
    if (off >= 0x400 && off < 0x800) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_TIM3_OFF + (off - 0x400));
    if (off >= 0x800 && off < 0xC00) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_TIM2_OFF + off);
    if (off >= 0x2000 && off < 0x2400) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_TIM14_OFF + (off - 0x2000));
    if (off >= 0x2C00 && off < 0x3000) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_WWDG_OFF + (off - 0x2C00));
    if (off >= 0x3000 && off < 0x3400) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_IWDG_OFF + (off - 0x3000));
    if (off >= 0x4400 && off < 0x4800) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_UART2_OFF + (off - 0x4400));
    if (off >= 0x5400 && off < 0x5800) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_I2C1_OFF + (off - 0x5400));
    if (off >= 0x7000 && off < 0x7400) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_PWR_OFF + (off - 0x7000));
    if (off >= 0x10000 && off < 0x10400) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_SYSCFG_OFF + (off - 0x10000));
    if (off >= 0x10400 && off < 0x10800) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_EXTI_OFF + (off - 0x10400));
    if (off >= 0x10800 && off < 0x10C00) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_GPIOA_OFF + (off - 0x10800));
    if (off >= 0x10C00 && off < 0x11000) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_GPIOB_OFF + (off - 0x10C00));
    if (off >= 0x11000 && off < 0x11400) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_GPIOF_OFF + (off - 0x11000));
    if (off >= 0x12400 && off < 0x12800) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_ADC_OFF + (off - 0x12400));
    if (off >= 0x12C00 && off < 0x13000) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_TIM1_OFF + (off - 0x12C00));
    if (off >= 0x13000 && off < 0x13400) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_SPI1_OFF + (off - 0x13000));
    if (off >= 0x13800 && off < 0x13C00) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_UART1_OFF + (off - 0x13800));
    if (off >= 0x14000 && off < 0x14400) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_TIM15_OFF + (off - 0x14000));
    if (off >= 0x20000 && off < 0x20400) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_DMA_OFF + (off - 0x20000));
    if (off >= 0x21000 && off < 0x21400) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_RCC_OFF + (off - 0x21000));
    if (off >= 0x22000 && off < 0x22400) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_FLASH_OFF + (off - 0x22000));
    if (off >= 0x23000 && off < 0x23400) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_CRC_OFF + (off - 0x23000));
    if (addr >= 0x48000000u && addr < 0x48000400u) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_GPIOA_OFF + (addr - 0x48000000u));
    if (addr >= 0x48000400u && addr < 0x48000800u) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_GPIOB_OFF + (addr - 0x48000400u));
    if (addr >= 0x48001400u && addr < 0x48001800u) return (int32_t)(CORTEX_EMU_PERIPH_OFFSET + PERIPH_GPIOF_OFF + (addr - 0x48001400u));
    return -1;
}

static int32_t translate_system_addr(uint32_t addr) {
    uint32_t off = addr - 0xE000E000u;
    if (off < 0x1000) return (int32_t)(CORTEX_EMU_SYSTEM_OFFSET + off);
    return -1;
}

int32_t emu_translate_addr(cortex_emu_t *emu, uint32_t addr) {
    if (addr < emu->flash_size) return (int32_t)(CORTEX_EMU_FLASH_OFFSET + addr);
    if (addr >= emu->flash_base && addr < emu->flash_base + emu->flash_size)
        return (int32_t)(CORTEX_EMU_FLASH_OFFSET + (addr - emu->flash_base));
    if (addr >= emu->sram_base && addr < emu->sram_base + emu->sram_size)
        return (int32_t)(CORTEX_EMU_SRAM_OFFSET + (addr - emu->sram_base));
    if (addr >= 0x40000000u && addr < 0x60000000u)
        return translate_periph_addr(emu, addr);
    if (addr >= 0xE0000000u && addr < 0xE0100000u)
        return translate_system_addr(addr);
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Bit-band support (Cortex-M3/M4)
 * ═══════════════════════════════════════════════════════════════════════ */

int emu_is_bitband(uint32_t addr) {
    return (addr >= 0x42000000u && addr < 0x44000000u)
        || (addr >= 0x22000000u && addr < 0x24000000u);
}

uint32_t emu_bitband_read(cortex_emu_t *emu, uint32_t alias) {
    uint32_t base = 0, region_base;
    if (alias >= 0x42000000u && alias < 0x44000000u) { base = 0x42000000u; region_base = 0x40000000u; }
    else if (alias >= 0x22000000u && alias < 0x24000000u) { base = 0x22000000u; region_base = 0x20000000u; }
    else return 0;
    uint32_t off = alias - base;
    uint32_t byte_off = off >> 5;
    uint32_t bit_num = (off >> 2) & 7;
    uint32_t word_addr = region_base + (byte_off & ~(uint32_t)3);
    uint32_t bit_in_word = ((byte_off & 3) << 3) + bit_num;
    return (cortex_emu_read32(emu, word_addr) >> bit_in_word) & 1;
}

void emu_bitband_write(cortex_emu_t *emu, uint32_t alias, uint32_t val) {
    uint32_t base = 0, region_base;
    if (alias >= 0x42000000u && alias < 0x44000000u) { base = 0x42000000u; region_base = 0x40000000u; }
    else if (alias >= 0x22000000u && alias < 0x24000000u) { base = 0x22000000u; region_base = 0x20000000u; }
    else return;
    uint32_t off = alias - base;
    uint32_t byte_off = off >> 5;
    uint32_t bit_num = (off >> 2) & 7;
    uint32_t word_addr = region_base + (byte_off & ~(uint32_t)3);
    uint32_t bit_in_word = ((byte_off & 3) << 3) + bit_num;
    uint32_t word = cortex_emu_read32(emu, word_addr);
    if (val & 1) word |= (1u << bit_in_word);
    else word &= ~(1u << bit_in_word);
    cortex_emu_write32(emu, word_addr, word);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Memory Access (address-space, routes through peripherals)
 * ═══════════════════════════════════════════════════════════════════════ */

uint32_t cortex_emu_read8(cortex_emu_t *emu, uint32_t addr) {
    if (emu_is_bitband(addr)) return emu_bitband_read(emu, addr);
    uint32_t sr = emu_periph_read(emu, addr);
    if (sr != PERIPH_NOT_HANDLED) return sr & 0xFF;
    int32_t off = emu_translate_addr(emu, addr);
    if (off < 0 || off >= CORTEX_EMU_MEM_SIZE) return 0;
    return (uint32_t)mem_load8(emu->mem, (uint32_t)off);
}

uint32_t cortex_emu_read16(cortex_emu_t *emu, uint32_t addr) {
    if (emu_is_bitband(addr)) return emu_bitband_read(emu, addr);
    uint32_t sr = emu_periph_read(emu, addr);
    if (sr != PERIPH_NOT_HANDLED) return sr & 0xFFFF;
    int32_t off = emu_translate_addr(emu, addr);
    if (off < 0 || off >= CORTEX_EMU_MEM_SIZE - 1) return 0;
    return (uint32_t)mem_load16(emu->mem, (uint32_t)off);
}

uint32_t cortex_emu_read32(cortex_emu_t *emu, uint32_t addr) {
    if (emu_is_bitband(addr)) return emu_bitband_read(emu, addr);
    uint32_t sr = emu_periph_read(emu, addr);
    if (sr != PERIPH_NOT_HANDLED) return sr;
    int32_t off = emu_translate_addr(emu, addr);
    if (off < 0 || off >= CORTEX_EMU_MEM_SIZE - 3) return 0;
    return mem_load32(emu->mem, (uint32_t)off);
}

void cortex_emu_write8(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    if (emu_is_bitband(addr)) { emu_bitband_write(emu, addr, val); return; }
    if (emu_periph_write(emu, addr, val)) return;
    if (emu->watch_enabled && addr == emu->watch_addr) emu->watch_triggered = 1;
    int32_t off = emu_translate_addr(emu, addr);
    if (off < 0 || off >= CORTEX_EMU_MEM_SIZE) return;
    if ((uint32_t)off < CORTEX_EMU_SRAM_OFFSET) return;
    mem_store8(emu->mem, (uint32_t)off, (uint8_t)(val & 0xFF));
}

void cortex_emu_write16(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    if (emu_is_bitband(addr)) { emu_bitband_write(emu, addr, val); return; }
    if (emu_periph_write(emu, addr, val)) return;
    if (emu->watch_enabled && addr == emu->watch_addr) emu->watch_triggered = 1;
    int32_t off = emu_translate_addr(emu, addr);
    if (off < 0 || off >= CORTEX_EMU_MEM_SIZE - 1) return;
    if ((uint32_t)off < CORTEX_EMU_SRAM_OFFSET) return;
    mem_store16(emu->mem, (uint32_t)off, (uint16_t)(val & 0xFFFF));
}

void cortex_emu_write32(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    if (emu_is_bitband(addr)) { emu_bitband_write(emu, addr, val); return; }
    if (emu_periph_write(emu, addr, val)) return;
    if (emu->watch_enabled && addr == emu->watch_addr) emu->watch_triggered = 1;
    int32_t off = emu_translate_addr(emu, addr);
    if (off < 0 || off >= CORTEX_EMU_MEM_SIZE - 3) return;
    if ((uint32_t)off < CORTEX_EMU_SRAM_OFFSET) return;
    mem_store32(emu->mem, (uint32_t)off, val);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Execution Loop (step / run)
 * ═══════════════════════════════════════════════════════════════════════ */

uint32_t cortex_emu_step(cortex_emu_t *emu) {
    if (emu->halted || emu->lockup) return 0;
    if (emu->sleeping) {
        if (emu->nvic_pending & emu->nvic_enabled) emu->sleeping = 0;
        else if (emu->nvic_pending & 0x80000000u) emu->sleeping = 0;
        else {
            emu_systick_tick(emu, 1);
            emu_timer_tick(emu, 1);
            emu->total_cycles++;
            return 1;
        }
    }

    emu_check_pending_interrupts(emu);
    uint32_t pc = emu->regs[15];
    emu->last_pc = pc;

    /* Record PC trace */
    emu->pc_trace[emu->pc_trace_idx] = pc;
    int tr_base = emu->pc_trace_idx * TRACE_REGS;
    for (int r = 0; r < 16; r++) emu->reg_trace_data[tr_base + r] = emu->regs[r];
    emu->reg_trace_data[tr_base + 16] = emu->xpsr;
    emu->reg_trace_data[tr_base + 17] = emu->primask;
    emu->pc_trace_idx = (emu->pc_trace_idx + 1) % PC_TRACE_SIZE;

    /* Validate PC is in executable memory */
    int pc_valid = (pc < emu->flash_size)
        || (pc >= emu->flash_base && pc < emu->flash_base + emu->flash_size)
        || (pc >= emu->sram_base && pc < emu->sram_base + emu->sram_size);
    if (!pc_valid) return emu_hard_fault(emu, CORTEX_EMU_FAULT_INVALID_PC);

    uint32_t hw = cortex_emu_read16(emu, pc);

    /* Breakpoint check */
    for (int i = 0; i < emu->breakpoint_count; i++) {
        if (emu->breakpoints[i] == pc) { emu->breakpoint_hit = 1; return 0; }
    }

    /* IT block conditional execution */
    if (emu->it_state & 0x0F) {
        uint32_t cond = emu->it_state >> 4;
        int cond_passed = emu_evaluate_condition(emu, cond);
        emu->it_state = (emu->it_state & 0xE0) | ((emu->it_state << 1) & 0x1F);
        if ((emu->it_state & 0x0F) == 0) emu->it_state = 0;
        if (!cond_passed) {
            if ((hw >> 11) >= 0x1D) emu->regs[15] = pc + 4;
            else emu->regs[15] = pc + 2;
            emu_systick_tick(emu, 1);
            emu_timer_tick(emu, 1);
            emu->total_cycles++;
            return 1;
        }
    }

    uint32_t cycles;
    if (hw == 0xFFFF) {
        uint32_t hw2 = cortex_emu_read16(emu, pc + 2);
        if (hw2 == 0xFFFF) { emu->regs[15] = pc + 4; cycles = 1; }
        else { emu->regs[15] = pc + 2; cycles = 1; }
    } else if ((hw >> 11) >= 0x1D) {
        uint32_t hw2 = cortex_emu_read16(emu, pc + 2);
        emu->regs[15] = pc + 4;
        cycles = emu_exec_thumb32(emu, hw, hw2);
    } else {
        emu->regs[15] = pc + 2;
        cycles = emu_exec_thumb16(emu, hw);
    }

    emu_systick_tick(emu, cycles);
    emu_timer_tick(emu, cycles);
    emu->total_cycles += (uint64_t)cycles;
    return cycles;
}

uint32_t cortex_emu_run(cortex_emu_t *emu, uint32_t max_cycles) {
    uint32_t cycles_run = 0;
    emu->breakpoint_hit = 0;
    emu->watch_triggered = 0;
    while (cycles_run < max_cycles) {
        if (emu->halted || emu->lockup || emu->breakpoint_hit || emu->watch_triggered) break;
        uint32_t c = cortex_emu_step(emu);
        if (c == 0) break;
        cycles_run += c;
    }
    return cycles_run;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Register Access
 * ═══════════════════════════════════════════════════════════════════════ */

uint32_t cortex_emu_get_reg(const cortex_emu_t *emu, uint32_t index) {
    if (index < 16) return emu->regs[index];
    if (index == 16) return emu->xpsr;
    if (index == 17) return emu->primask;
    if (index == 18) return emu->control;
    if (index == 19) return emu->msp;
    if (index == 20) return emu->psp;
    return 0;
}

void cortex_emu_set_reg(cortex_emu_t *emu, uint32_t index, uint32_t value) {
    if (index < 16) { emu->regs[index] = value; return; }
    if (index == 16) { emu->xpsr = value; return; }
    if (index == 17) { emu->primask = value; return; }
    if (index == 18) { emu->control = value; return; }
    if (index == 19) { emu->msp = value; return; }
    if (index == 20) { emu->psp = value; return; }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Debugger: Breakpoints & Watchpoints
 * ═══════════════════════════════════════════════════════════════════════ */

int cortex_emu_add_breakpoint(cortex_emu_t *emu, uint32_t addr) {
    if (emu->breakpoint_count >= MAX_BREAKPOINTS) return 0;
    emu->breakpoints[emu->breakpoint_count++] = addr;
    return 1;
}

int cortex_emu_remove_breakpoint(cortex_emu_t *emu, uint32_t addr) {
    for (int i = 0; i < emu->breakpoint_count; i++) {
        if (emu->breakpoints[i] == addr) {
            for (int j = i; j < emu->breakpoint_count - 1; j++)
                emu->breakpoints[j] = emu->breakpoints[j + 1];
            emu->breakpoint_count--;
            return 1;
        }
    }
    return 0;
}

void cortex_emu_clear_breakpoints(cortex_emu_t *emu) { emu->breakpoint_count = 0; }

void cortex_emu_set_watchpoint(cortex_emu_t *emu, uint32_t addr) {
    emu->watch_addr = addr; emu->watch_enabled = 1; emu->watch_triggered = 0;
}

void cortex_emu_clear_watchpoint(cortex_emu_t *emu) {
    emu->watch_enabled = 0; emu->watch_triggered = 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Status & Diagnostics (public API)
 * ═══════════════════════════════════════════════════════════════════════ */

int      cortex_emu_is_halted(const cortex_emu_t *e)          { return e->halted; }
int      cortex_emu_is_sleeping(const cortex_emu_t *e)        { return e->sleeping; }
int      cortex_emu_is_lockup(const cortex_emu_t *e)          { return e->lockup; }
int      cortex_emu_is_breakpoint_hit(const cortex_emu_t *e)  { return e->breakpoint_hit; }
int      cortex_emu_is_watch_triggered(const cortex_emu_t *e) { return e->watch_triggered; }
uint32_t cortex_emu_get_last_pc(const cortex_emu_t *e)        { return e->last_pc; }
uint64_t cortex_emu_get_total_cycles(const cortex_emu_t *e)   { return e->total_cycles; }
uint32_t cortex_emu_get_active_exception(const cortex_emu_t *e) { return (uint32_t)e->active_exception; }
void     cortex_emu_set_halted(cortex_emu_t *e, int h)        { e->halted = h; }
uint32_t cortex_emu_get_fault_count(const cortex_emu_t *e)    { return e->hard_fault_count; }
uint32_t cortex_emu_get_fault_code(const cortex_emu_t *e)     { return e->hard_fault_code; }
uint32_t cortex_emu_get_reset_count(const cortex_emu_t *e)    { return e->reset_count; }
uint32_t cortex_emu_get_systick_fire_count(const cortex_emu_t *e) { return e->systick_fire_count; }
uint32_t cortex_emu_get_exception_entry_count(const cortex_emu_t *e) { return e->exception_entry_count; }
uint32_t cortex_emu_get_exception_exit_count(const cortex_emu_t *e) { return e->exception_exit_count; }

uint32_t cortex_emu_get_pc_trace(const cortex_emu_t *e, uint32_t i) {
    if (i >= PC_TRACE_SIZE) return 0;
    int idx = (e->pc_trace_idx + (int)i) % PC_TRACE_SIZE;
    return e->pc_trace[idx];
}

uint32_t cortex_emu_get_reg_trace(const cortex_emu_t *e, uint32_t entry, uint32_t reg_idx) {
    if (entry >= PC_TRACE_SIZE || reg_idx >= TRACE_REGS) return 0;
    int idx = ((e->pc_trace_idx + (int)entry) % PC_TRACE_SIZE) * TRACE_REGS + (int)reg_idx;
    return e->reg_trace_data[idx];
}

uint32_t cortex_emu_get_shadow_stack_size(const cortex_emu_t *e) { return (uint32_t)e->shadow_stack_count; }

uint32_t cortex_emu_get_shadow_stack_pc(const cortex_emu_t *e, uint32_t i) {
    if (i >= (uint32_t)e->shadow_stack_count) return 0;
    int oldest = e->shadow_stack_count < SHADOW_STACK_SIZE ? 0 : e->shadow_stack_idx;
    int idx = (oldest + (int)i) % SHADOW_STACK_SIZE;
    return e->shadow_stack_pc[idx];
}

uint32_t cortex_emu_get_shadow_stack_sp(const cortex_emu_t *e, uint32_t i) {
    if (i >= (uint32_t)e->shadow_stack_count) return 0;
    int oldest = e->shadow_stack_count < SHADOW_STACK_SIZE ? 0 : e->shadow_stack_idx;
    int idx = (oldest + (int)i) % SHADOW_STACK_SIZE;
    return e->shadow_stack_sp[idx];
}

/* ═══════════════════════════════════════════════════════════════════════
 *  GPIO / ADC / UART / SPI / I2C / EXTI / DMA / Timer public API
 * ═══════════════════════════════════════════════════════════════════════ */

void cortex_emu_set_gpio_input(cortex_emu_t *e, uint32_t port, uint32_t val) {
    if (port < GPIO_PORTS) e->gpio_idr[port] = val;
}
uint32_t cortex_emu_get_gpio_input(const cortex_emu_t *e, uint32_t port) {
    return port < GPIO_PORTS ? e->gpio_idr[port] : 0;
}
uint32_t cortex_emu_get_gpio_output(const cortex_emu_t *e, uint32_t port) {
    return port < GPIO_PORTS ? e->gpio_odr[port] : 0;
}
uint32_t cortex_emu_get_gpio_mode(const cortex_emu_t *e, uint32_t port) {
    return port < GPIO_PORTS ? e->gpio_moder[port] : 0;
}
uint32_t cortex_emu_get_gpio_crl(const cortex_emu_t *e, uint32_t port) {
    return port < GPIO_PORTS ? e->gpio_crl[port] : 0;
}
uint32_t cortex_emu_get_gpio_crh(const cortex_emu_t *e, uint32_t port) {
    return port < GPIO_PORTS ? e->gpio_crh[port] : 0;
}

void cortex_emu_set_adc_value(cortex_emu_t *e, uint32_t val) { e->adc_dr = val & 0xFFF; }
void cortex_emu_set_adc_channel_value(cortex_emu_t *e, uint32_t ch, uint32_t val) {
    if ((int32_t)ch < ADC_CHANNELS) e->adc_channel_values[ch] = val & 0xFFF;
    e->adc_dr = val & 0xFFF;
}

void cortex_emu_uart_receive(cortex_emu_t *e, uint32_t idx, uint32_t data) {
    if ((int32_t)idx >= UART_COUNT) return;
    int u = (int)idx;
    e->uart_rdr[u] = data & 0x1FF;
    e->uart_isr[u] |= (1u << 5);
    if (e->uart_cr1[u] & (1u << 5)) {
        uint32_t irq = u == 0 ? 27 : 28;
        e->nvic_pending |= (1u << irq);
    }
}

int32_t cortex_emu_uart_read_tx(cortex_emu_t *e, uint32_t idx) {
    if ((int32_t)idx >= UART_COUNT) return -1;
    int u = (int)idx;
    if (e->uart_tx_count[u] == 0) return -1;
    int tail = e->uart_tx_tail[u];
    int32_t data = (int32_t)e->uart_tx_buf[u * UART_TX_BUF_SIZE + tail];
    e->uart_tx_tail[u] = (tail + 1) % UART_TX_BUF_SIZE;
    e->uart_tx_count[u]--;
    return data;
}

int32_t cortex_emu_uart_tx_pending(const cortex_emu_t *e, uint32_t idx) {
    if ((int32_t)idx >= UART_COUNT) return 0;
    return e->uart_tx_count[idx];
}

void cortex_emu_set_spi_rx(cortex_emu_t *e, uint32_t data) {
    e->spi_rx_data = data & 0xFFFF;
    e->spi_sr |= 1;
    if (e->spi_cr2 & (1u << 6)) e->nvic_pending |= (1u << 25);
}

uint32_t cortex_emu_get_spi_tx(const cortex_emu_t *e) { return e->spi_dr; }

void cortex_emu_i2c_receive(cortex_emu_t *e, uint32_t data) {
    e->i2c_rxdr = data & 0xFF;
    e->i2c_isr |= (1u << 2);
    if (e->i2c_cr1 & (1u << 2)) e->nvic_pending |= (1u << 23);
}

uint32_t cortex_emu_get_i2c_tx(const cortex_emu_t *e) { return e->i2c_txdr; }

void cortex_emu_trigger_irq(cortex_emu_t *e, uint32_t irq) {
    if (irq < (uint32_t)MAX_IRQ) e->nvic_pending |= (1u << irq);
}

void cortex_emu_trigger_exti(cortex_emu_t *e, uint32_t line, int rising) {
    uint32_t mask = 1u << line;
    int triggered = 0;
    if (rising && (e->exti_rtsr & mask)) triggered = 1;
    if (!rising && (e->exti_ftsr & mask)) triggered = 1;
    if (triggered && (e->exti_imr & mask)) {
        e->exti_pr |= mask;
        if (line <= 1)       e->nvic_pending |= (1u << 5);
        else if (line <= 3)  e->nvic_pending |= (1u << 6);
        else                 e->nvic_pending |= (1u << 7);
    }
}

void cortex_emu_inject_tim3_capture(cortex_emu_t *e, uint32_t period, uint32_t pulse_width) {
    int t = 1; /* TIM3 = index 1 */
    e->tim_ccr[t * 4 + 0] = period;
    e->tim_ccr[t * 4 + 1] = pulse_width;
    uint32_t sr = e->tim_sr[t];
    sr |= (1u << 1) | (1u << 2);
    uint32_t smcr = e->tim_smcr[t];
    if ((smcr & 7) == 4) { e->tim_cnt[t] = 0; sr &= ~1u; }
    e->tim_sr[t] = sr;
    uint32_t dier = e->tim_dier[t];
    if ((sr & dier) != 0) e->nvic_pending |= (1u << 16);
}

void cortex_emu_dma_step(cortex_emu_t *e) { emu_dma_tick(e); }

void cortex_emu_gpio_trace_start(cortex_emu_t *e) { e->gpio_trace_idx = 0; e->gpio_trace_enabled = 1; }
void cortex_emu_gpio_trace_stop(cortex_emu_t *e)  { e->gpio_trace_enabled = 0; }
uint32_t cortex_emu_gpio_trace_count(const cortex_emu_t *e) { return (uint32_t)e->gpio_trace_idx; }
uint32_t cortex_emu_gpio_trace_get(const cortex_emu_t *e, uint32_t i, uint32_t field) {
    int base = (int)i * 3;
    if (base + (int)field < GPIO_TRACE_MAX * 3) return e->gpio_trace_log[base + (int)field];
    return 0;
}

uint32_t cortex_emu_get_tim1_eff_ccmr1(const cortex_emu_t *e) { return e->tim_ccmr1_eff[0]; }
uint32_t cortex_emu_get_tim1_eff_ccmr2(const cortex_emu_t *e) { return e->tim_ccmr2_eff[0]; }
uint32_t cortex_emu_get_tim1_eff_ccer(const cortex_emu_t *e)  { return e->tim_ccer_eff[0]; }

uint32_t       cortex_emu_get_mem_size(void)    { return CORTEX_EMU_MEM_SIZE; }
uint32_t       cortex_emu_get_flash_offset(void) { return CORTEX_EMU_FLASH_OFFSET; }
uint32_t       cortex_emu_get_sram_offset(void)  { return CORTEX_EMU_SRAM_OFFSET; }
const uint8_t *cortex_emu_get_mem_ptr(const cortex_emu_t *e) { return e->mem; }
uint32_t       cortex_emu_get_it_state(const cortex_emu_t *e) { return e->it_state; }
uint32_t       cortex_emu_get_xpsr(const cortex_emu_t *e) { return e->xpsr; }
