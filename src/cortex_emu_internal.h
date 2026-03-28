/*
 * cortex-emu — Internal header (not part of public API)
 *
 * Contains the full emulator state struct definition and internal
 * function declarations shared between translation units.
 */

#ifndef CORTEX_EMU_INTERNAL_H
#define CORTEX_EMU_INTERNAL_H

#include "cortex_emu.h"
#include <string.h>

/* ── Internal constants ──────────────────────────────────────────────── */
#define REG_COUNT          16
#define MAX_BREAKPOINTS    32
#define MAX_IRQ            32
#define PC_TRACE_SIZE      16
#define TRACE_REGS         18   /* R0-R15 + xPSR + PRIMASK */
#define SHADOW_STACK_SIZE  64
#define STACK_GUARD_SIZE   64
#define GPIO_PORTS         4    /* A, B, C, F */
#define DMA_CHANNELS       5
#define TIM_N              6    /* TIM1, TIM3, TIM15, TIM2, TIM4, TIM14 */
#define UART_COUNT         2
#define UART_TX_BUF_SIZE   16
#define ADC_CHANNELS       16
#define GPIO_TRACE_MAX     256

#define PERIPH_NOT_HANDLED 0xDEAD0000u

/* Peripheral sub-offsets (relative to CORTEX_EMU_PERIPH_OFFSET) */
#define PERIPH_TIM2_OFF    0x0000
#define PERIPH_TIM3_OFF    0x0400
#define PERIPH_RCC_OFF     0x1000
#define PERIPH_FLASH_OFF   0x2000
#define PERIPH_GPIOA_OFF   0x3000
#define PERIPH_GPIOB_OFF   0x3400
#define PERIPH_GPIOF_OFF   0x3800
#define PERIPH_TIM1_OFF    0x4000
#define PERIPH_ADC_OFF     0x4400
#define PERIPH_DMA_OFF     0x5000
#define PERIPH_TIM14_OFF   0x5800
#define PERIPH_TIM15_OFF   0x5C00
#define PERIPH_UART1_OFF   0x6000
#define PERIPH_UART2_OFF   0x6400
#define PERIPH_SPI1_OFF    0x6800
#define PERIPH_I2C1_OFF    0x6C00
#define PERIPH_IWDG_OFF    0x7000
#define PERIPH_WWDG_OFF    0x7400
#define PERIPH_EXTI_OFF    0x7800
#define PERIPH_SYSCFG_OFF  0x7C00
#define PERIPH_PWR_OFF     0x8000
#define PERIPH_CRC_OFF     0x8400

/* ── Full emulator state ─────────────────────────────────────────────── */
struct cortex_emu {
    /* ── CPU Registers ──────────────────────────────────────────────── */
    uint32_t regs[REG_COUNT];
    uint32_t xpsr;
    uint32_t primask;
    uint32_t control;
    uint32_t basepri;
    uint32_t faultmask;
    uint32_t msp;
    uint32_t psp;
    uint32_t it_state;

    /* ── CPU State ──────────────────────────────────────────────────── */
    uint32_t core_type;
    int      halted;
    int      sleeping;
    int      lockup;
    int32_t  active_exception;
    uint64_t total_cycles;
    uint32_t last_pc;

    /* ── Memory Configuration ───────────────────────────────────────── */
    uint32_t flash_base;
    uint32_t flash_size;
    uint32_t sram_base;
    uint32_t sram_size;

    /* ── NVIC ───────────────────────────────────────────────────────── */
    uint32_t nvic_enabled;
    uint32_t nvic_pending;
    uint32_t nvic_iabr;
    uint32_t nvic_ipr[8];
    uint32_t scb_shpr2;
    uint32_t scb_shpr3;

    /* ── SysTick ────────────────────────────────────────────────────── */
    uint32_t systick_ctrl;
    uint32_t systick_load;
    uint32_t systick_val;
    uint32_t systick_calib;
    uint32_t systick_fire_count;

    /* ── DMA ────────────────────────────────────────────────────────── */
    uint32_t dma_isr;
    uint32_t dma_ccr[DMA_CHANNELS];
    uint32_t dma_cndtr[DMA_CHANNELS];
    uint32_t dma_cpar[DMA_CHANNELS];
    uint32_t dma_cmar[DMA_CHANNELS];

    /* ── GPIO ───────────────────────────────────────────────────────── */
    uint32_t gpio_idr[GPIO_PORTS];
    uint32_t gpio_odr[GPIO_PORTS];
    uint32_t gpio_moder[GPIO_PORTS];
    uint32_t gpio_crl[GPIO_PORTS];
    uint32_t gpio_crh[GPIO_PORTS];
    uint32_t gpio_ospeedr[GPIO_PORTS];
    uint32_t gpio_pupdr[GPIO_PORTS];
    uint32_t gpio_otyper[GPIO_PORTS];
    uint32_t gpio_lckr[GPIO_PORTS];
    uint32_t gpio_afrl[GPIO_PORTS];
    uint32_t gpio_afrh[GPIO_PORTS];

    /* ── RCC ────────────────────────────────────────────────────────── */
    uint32_t rcc_cr;
    uint32_t rcc_cfgr;
    uint32_t rcc_cir;
    uint32_t rcc_apb2rstr;
    uint32_t rcc_apb1rstr;
    uint32_t rcc_ahbenr;
    uint32_t rcc_apb2enr;
    uint32_t rcc_apb1enr;

    /* ── ADC ────────────────────────────────────────────────────────── */
    uint32_t adc_isr;
    uint32_t adc_ier;
    uint32_t adc_cr;
    uint32_t adc_cfgr1;
    uint32_t adc_smpr;
    uint32_t adc_chselr;
    uint32_t adc_dr;
    uint32_t adc_sqr1;
    uint32_t adc_sqr3;
    uint32_t adc_channel_values[ADC_CHANNELS];

    /* ── Flash Interface ────────────────────────────────────────────── */
    uint32_t flash_acr;

    /* ── Timers ─────────────────────────────────────────────────────── */
    uint32_t tim_cr1[TIM_N];
    uint32_t tim_cr2[TIM_N];
    uint32_t tim_smcr[TIM_N];
    uint32_t tim_dier[TIM_N];
    uint32_t tim_sr[TIM_N];
    uint32_t tim_ccmr1[TIM_N];
    uint32_t tim_ccmr2[TIM_N];
    uint32_t tim_ccer[TIM_N];
    uint32_t tim_cnt[TIM_N];
    uint32_t tim_psc[TIM_N];
    uint32_t tim_arr[TIM_N];
    uint32_t tim_rcr[TIM_N];
    uint32_t tim_ccr[TIM_N * 4];
    uint32_t tim_bdtr[TIM_N];
    /* CCPC shadow (effective) registers */
    uint32_t tim_ccmr1_eff[TIM_N];
    uint32_t tim_ccmr2_eff[TIM_N];
    uint32_t tim_ccer_eff[TIM_N];
    uint32_t tim_rcr_cnt[TIM_N];
    /* Internal timer state */
    uint32_t tim_pscnt[TIM_N];
    int32_t  tim_dir[TIM_N];

    /* ── UART ───────────────────────────────────────────────────────── */
    uint32_t uart_cr1[UART_COUNT];
    uint32_t uart_cr2[UART_COUNT];
    uint32_t uart_cr3[UART_COUNT];
    uint32_t uart_brr[UART_COUNT];
    uint32_t uart_gtpr[UART_COUNT];
    uint32_t uart_rtor[UART_COUNT];
    uint32_t uart_rqr[UART_COUNT];
    uint32_t uart_isr[UART_COUNT];
    uint32_t uart_icr[UART_COUNT];
    uint32_t uart_rdr[UART_COUNT];
    uint32_t uart_tdr[UART_COUNT];
    uint8_t  uart_tx_buf[UART_COUNT * UART_TX_BUF_SIZE];
    int32_t  uart_tx_head[UART_COUNT];
    int32_t  uart_tx_tail[UART_COUNT];
    int32_t  uart_tx_count[UART_COUNT];

    /* ── SPI ────────────────────────────────────────────────────────── */
    uint32_t spi_cr1;
    uint32_t spi_cr2;
    uint32_t spi_sr;
    uint32_t spi_dr;
    uint32_t spi_crcpr;
    uint32_t spi_rxcrcr;
    uint32_t spi_txcrcr;
    uint32_t spi_i2scfgr;
    uint32_t spi_i2spr;
    uint32_t spi_rx_data;

    /* ── I2C ────────────────────────────────────────────────────────── */
    uint32_t i2c_cr1;
    uint32_t i2c_cr2;
    uint32_t i2c_oar1;
    uint32_t i2c_oar2;
    uint32_t i2c_timingr;
    uint32_t i2c_timeoutr;
    uint32_t i2c_isr;
    uint32_t i2c_icr;
    uint32_t i2c_pecr;
    uint32_t i2c_rxdr;
    uint32_t i2c_txdr;

    /* ── IWDG ───────────────────────────────────────────────────────── */
    uint32_t iwdg_kr;
    uint32_t iwdg_pr;
    uint32_t iwdg_rlr;
    uint32_t iwdg_sr;
    uint32_t iwdg_winr;
    uint32_t iwdg_counter;
    int      iwdg_running;
    int      iwdg_unlocked;

    /* ── WWDG ───────────────────────────────────────────────────────── */
    uint32_t wwdg_cr;
    uint32_t wwdg_cfr;
    uint32_t wwdg_sr;

    /* ── EXTI ───────────────────────────────────────────────────────── */
    uint32_t exti_imr;
    uint32_t exti_emr;
    uint32_t exti_rtsr;
    uint32_t exti_ftsr;
    uint32_t exti_swier;
    uint32_t exti_pr;

    /* ── SYSCFG ─────────────────────────────────────────────────────── */
    uint32_t syscfg_cfgr1;
    uint32_t syscfg_exticr[4];
    uint32_t syscfg_cfgr2;

    /* ── PWR ────────────────────────────────────────────────────────── */
    uint32_t pwr_cr;
    uint32_t pwr_csr;

    /* ── CRC ────────────────────────────────────────────────────────── */
    uint32_t crc_dr;
    uint32_t crc_idr;
    uint32_t crc_cr;
    uint32_t crc_init;

    /* ── Debugger State ─────────────────────────────────────────────── */
    uint32_t breakpoints[MAX_BREAKPOINTS];
    int32_t  breakpoint_count;
    int      breakpoint_hit;
    uint32_t watch_addr;
    int      watch_enabled;
    int      watch_triggered;

    /* ── Diagnostic Counters ────────────────────────────────────────── */
    uint32_t hard_fault_count;
    uint32_t hard_fault_code;
    uint32_t reset_count;
    uint32_t exception_entry_count;
    uint32_t exception_exit_count;

    /* ── Shadow Stack ───────────────────────────────────────────────── */
    uint32_t shadow_stack_pc[SHADOW_STACK_SIZE];
    uint32_t shadow_stack_sp[SHADOW_STACK_SIZE];
    int32_t  shadow_stack_idx;
    int32_t  shadow_stack_count;

    /* ── PC / Register Trace ────────────────────────────────────────── */
    uint32_t pc_trace[PC_TRACE_SIZE];
    uint32_t reg_trace_data[PC_TRACE_SIZE * TRACE_REGS];
    int32_t  pc_trace_idx;

    /* ── GPIO Write Trace ───────────────────────────────────────────── */
    uint32_t gpio_trace_log[GPIO_TRACE_MAX * 3];
    int32_t  gpio_trace_idx;
    int      gpio_trace_enabled;

    /* ── Flat Memory Buffer (MUST be last — ~768 KB) ────────────────── */
    uint8_t  mem[CORTEX_EMU_MEM_SIZE];
};

/* ═══════════════════════════════════════════════════════════════════════
 *  Internal memory access helpers (host byte order, little-endian ARM)
 * ═══════════════════════════════════════════════════════════════════════ */

static inline uint8_t mem_load8(const uint8_t *buf, uint32_t off) {
    return buf[off];
}

static inline uint16_t mem_load16(const uint8_t *buf, uint32_t off) {
    uint16_t v;
    memcpy(&v, buf + off, 2);
    return v;
}

static inline uint32_t mem_load32(const uint8_t *buf, uint32_t off) {
    uint32_t v;
    memcpy(&v, buf + off, 4);
    return v;
}

static inline void mem_store8(uint8_t *buf, uint32_t off, uint8_t v) {
    buf[off] = v;
}

static inline void mem_store16(uint8_t *buf, uint32_t off, uint16_t v) {
    memcpy(buf + off, &v, 2);
}

static inline void mem_store32(uint8_t *buf, uint32_t off, uint32_t v) {
    memcpy(buf + off, &v, 4);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Flag helpers
 * ═══════════════════════════════════════════════════════════════════════ */

static inline int flagN(const cortex_emu_t *e) { return (e->xpsr & 0x80000000u) != 0; }
static inline int flagZ(const cortex_emu_t *e) { return (e->xpsr & 0x40000000u) != 0; }
static inline int flagC(const cortex_emu_t *e) { return (e->xpsr & 0x20000000u) != 0; }
static inline int flagV(const cortex_emu_t *e) { return (e->xpsr & 0x10000000u) != 0; }

static inline void setNZ(cortex_emu_t *e, uint32_t result) {
    e->xpsr &= ~0xC0000000u;
    if (result & 0x80000000u) e->xpsr |= 0x80000000u;
    if (result == 0)          e->xpsr |= 0x40000000u;
}

static inline void setNZC(cortex_emu_t *e, uint32_t result, int carry) {
    e->xpsr &= ~0xE0000000u;
    if (result & 0x80000000u) e->xpsr |= 0x80000000u;
    if (result == 0)          e->xpsr |= 0x40000000u;
    if (carry)                e->xpsr |= 0x20000000u;
}

static inline uint32_t addWithCarry(cortex_emu_t *e, uint32_t a, uint32_t b, uint32_t carryIn) {
    uint64_t r64 = (uint64_t)a + (uint64_t)b + (uint64_t)carryIn;
    uint32_t result = (uint32_t)r64;
    int carry = r64 > 0xFFFFFFFFu;
    int overflow = (int)(((a ^ result) & (b ^ result) & 0x80000000u) != 0);
    e->xpsr &= ~0xF0000000u;
    if (result & 0x80000000u) e->xpsr |= 0x80000000u;
    if (result == 0)          e->xpsr |= 0x40000000u;
    if (carry)                e->xpsr |= 0x20000000u;
    if (overflow)             e->xpsr |= 0x10000000u;
    return result;
}

static inline uint32_t addNoFlags(uint32_t a, uint32_t b, uint32_t carryIn) {
    return (uint32_t)((uint64_t)a + (uint64_t)b + (uint64_t)carryIn);
}

static inline uint32_t signedSat32(cortex_emu_t *e, int64_t val) {
    if (val > (int64_t)0x7FFFFFFF)  { e->xpsr |= (1u << 27); return 0x7FFFFFFFu; }
    if (val < (int64_t)-0x80000000LL) { e->xpsr |= (1u << 27); return (uint32_t)(int32_t)(-0x80000000LL); }
    return (uint32_t)(int32_t)val;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Internal function declarations (cross-file)
 * ═══════════════════════════════════════════════════════════════════════ */

/* Address translation */
int32_t  emu_translate_addr(cortex_emu_t *emu, uint32_t addr);

/* Memory I/O (public API wrappers call these, instruction decoders use them) */
/* These are the public cortex_emu_read/write functions declared in cortex_emu.h */

/* Peripheral dispatch */
uint32_t emu_periph_read(cortex_emu_t *emu, uint32_t addr);
int      emu_periph_write(cortex_emu_t *emu, uint32_t addr, uint32_t val);

/* Individual peripherals */
uint32_t emu_nvic_read(cortex_emu_t *emu, uint32_t addr);
void     emu_nvic_write(cortex_emu_t *emu, uint32_t addr, uint32_t val);
uint32_t emu_systick_read(cortex_emu_t *emu, uint32_t addr);
void     emu_systick_write(cortex_emu_t *emu, uint32_t addr, uint32_t val);
void     emu_systick_tick(cortex_emu_t *emu, uint32_t n);
uint32_t emu_scb_read(cortex_emu_t *emu, uint32_t addr);
void     emu_scb_write(cortex_emu_t *emu, uint32_t addr, uint32_t val);
uint32_t emu_gpio_read(cortex_emu_t *emu, uint32_t addr);
void     emu_gpio_write(cortex_emu_t *emu, uint32_t addr, uint32_t val);
uint32_t emu_rcc_read(cortex_emu_t *emu, uint32_t addr);
void     emu_rcc_write(cortex_emu_t *emu, uint32_t addr, uint32_t val);
uint32_t emu_adc_read(cortex_emu_t *emu, uint32_t addr);
void     emu_adc_write(cortex_emu_t *emu, uint32_t addr, uint32_t val);
uint32_t emu_flash_read(cortex_emu_t *emu, uint32_t addr);
void     emu_flash_write(cortex_emu_t *emu, uint32_t addr, uint32_t val);
int32_t  emu_timer_index(uint32_t addr);
uint32_t emu_timer_read(cortex_emu_t *emu, uint32_t addr);
void     emu_timer_write(cortex_emu_t *emu, uint32_t addr, uint32_t val);
void     emu_timer_tick(cortex_emu_t *emu, uint32_t ticks);
void     emu_timer_reset(cortex_emu_t *emu);
uint32_t emu_dma_read(cortex_emu_t *emu, uint32_t addr);
void     emu_dma_write(cortex_emu_t *emu, uint32_t addr, uint32_t val);
void     emu_dma_tick(cortex_emu_t *emu);
int32_t  emu_uart_index(uint32_t addr);
uint32_t emu_uart_read(cortex_emu_t *emu, uint32_t addr);
void     emu_uart_write(cortex_emu_t *emu, uint32_t addr, uint32_t val);
void     emu_uart_reset(cortex_emu_t *emu);
uint32_t emu_spi_read(cortex_emu_t *emu, uint32_t addr);
void     emu_spi_write(cortex_emu_t *emu, uint32_t addr, uint32_t val);
void     emu_spi_reset(cortex_emu_t *emu);
uint32_t emu_i2c_read(cortex_emu_t *emu, uint32_t addr);
void     emu_i2c_write(cortex_emu_t *emu, uint32_t addr, uint32_t val);
void     emu_i2c_reset(cortex_emu_t *emu);
uint32_t emu_iwdg_read(cortex_emu_t *emu, uint32_t addr);
void     emu_iwdg_write(cortex_emu_t *emu, uint32_t addr, uint32_t val);
void     emu_iwdg_reset(cortex_emu_t *emu);
uint32_t emu_wwdg_read(cortex_emu_t *emu, uint32_t addr);
void     emu_wwdg_write(cortex_emu_t *emu, uint32_t addr, uint32_t val);
void     emu_wwdg_reset(cortex_emu_t *emu);
uint32_t emu_exti_read(cortex_emu_t *emu, uint32_t addr);
void     emu_exti_write(cortex_emu_t *emu, uint32_t addr, uint32_t val);
void     emu_exti_reset(cortex_emu_t *emu);
uint32_t emu_syscfg_read(cortex_emu_t *emu, uint32_t addr);
void     emu_syscfg_write(cortex_emu_t *emu, uint32_t addr, uint32_t val);
void     emu_syscfg_reset(cortex_emu_t *emu);
uint32_t emu_pwr_read(cortex_emu_t *emu, uint32_t addr);
void     emu_pwr_write(cortex_emu_t *emu, uint32_t addr, uint32_t val);
void     emu_pwr_reset(cortex_emu_t *emu);
uint32_t emu_crc_read(cortex_emu_t *emu, uint32_t addr);
void     emu_crc_write(cortex_emu_t *emu, uint32_t addr, uint32_t val);
void     emu_crc_reset(cortex_emu_t *emu);

/* Exception handling */
void     emu_enter_exception(cortex_emu_t *emu, int32_t exc_num);
void     emu_exit_exception(cortex_emu_t *emu);
void     emu_check_pending_interrupts(cortex_emu_t *emu);
uint32_t emu_hard_fault(cortex_emu_t *emu, uint32_t code);
void     emu_set_pending_exception(cortex_emu_t *emu, int32_t exc_num);
uint32_t emu_get_irq_priority(const cortex_emu_t *emu, int32_t irqn);
uint32_t emu_get_sys_exception_priority(const cortex_emu_t *emu, int32_t exc_num);
uint32_t emu_get_active_exception_priority(const cortex_emu_t *emu);
int      emu_evaluate_condition(const cortex_emu_t *emu, uint32_t cond);

/* Shadow stack */
static inline void emu_record_shadow_stack(cortex_emu_t *e, uint32_t pc, uint32_t sp) {
    e->shadow_stack_pc[e->shadow_stack_idx] = pc;
    e->shadow_stack_sp[e->shadow_stack_idx] = sp;
    e->shadow_stack_idx = (e->shadow_stack_idx + 1) % SHADOW_STACK_SIZE;
    if (e->shadow_stack_count < SHADOW_STACK_SIZE) e->shadow_stack_count++;
}

static inline int emu_check_stack_overflow(const cortex_emu_t *e, uint32_t sp) {
    return sp < e->sram_base + STACK_GUARD_SIZE || sp > e->sram_base + e->sram_size;
}

/* GPIO trace */
static inline void emu_gpio_trace_record(cortex_emu_t *e, int port, uint32_t reg, uint32_t val) {
    if (!e->gpio_trace_enabled || e->gpio_trace_idx >= GPIO_TRACE_MAX) return;
    int base = e->gpio_trace_idx * 3;
    e->gpio_trace_log[base]     = (uint32_t)port;
    e->gpio_trace_log[base + 1] = reg;
    e->gpio_trace_log[base + 2] = val;
    e->gpio_trace_idx++;
}

/* Instruction decoders */
uint32_t emu_exec_thumb16(cortex_emu_t *emu, uint32_t hw);
uint32_t emu_exec_thumb32(cortex_emu_t *emu, uint32_t hw1, uint32_t hw2);

/* Bit-band helpers (M3/M4) */
int      emu_is_bitband(uint32_t addr);
uint32_t emu_bitband_read(cortex_emu_t *emu, uint32_t alias_addr);
void     emu_bitband_write(cortex_emu_t *emu, uint32_t alias_addr, uint32_t val);

#endif /* CORTEX_EMU_INTERNAL_H */
