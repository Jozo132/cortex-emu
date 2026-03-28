#include "cortex_emu_internal.h"

/* Timer base addresses: 0=TIM1, 1=TIM3, 2=TIM15, 3=TIM2, 4=TIM4, 5=TIM14 */
int32_t emu_timer_index(uint32_t addr) {
    if (addr >= 0x40012C00u && addr < 0x40013000u) return 0;  /* TIM1  */
    if (addr >= 0x40000400u && addr < 0x40000800u) return 1;  /* TIM3  */
    if (addr >= 0x40014000u && addr < 0x40014400u) return 2;  /* TIM15 */
    if (addr >= 0x40000000u && addr < 0x40000400u) return 3;  /* TIM2  */
    if (addr >= 0x40000800u && addr < 0x40000C00u) return 4;  /* TIM4  */
    if (addr >= 0x40002000u && addr < 0x40002400u) return 5;  /* TIM14 */
    return -1;
}

uint32_t emu_timer_read(cortex_emu_t *emu, uint32_t addr) {
    int32_t t = emu_timer_index(addr);
    if (t < 0) return 0;
    uint32_t r = addr & 0x3FF;
    if (r == 0x00) return emu->tim_cr1[t];
    if (r == 0x04) return emu->tim_cr2[t];
    if (r == 0x08) return emu->tim_smcr[t];
    if (r == 0x0C) return emu->tim_dier[t];
    if (r == 0x10) return emu->tim_sr[t];
    if (r == 0x18) return emu->tim_ccmr1[t];
    if (r == 0x1C) return emu->tim_ccmr2[t];
    if (r == 0x20) return emu->tim_ccer[t];
    if (r == 0x24) return emu->tim_cnt[t];
    if (r == 0x28) return emu->tim_psc[t];
    if (r == 0x2C) return emu->tim_arr[t];
    if (r == 0x30) return emu->tim_rcr[t];
    if (r == 0x34) return emu->tim_ccr[t * 4 + 0];
    if (r == 0x38) return emu->tim_ccr[t * 4 + 1];
    if (r == 0x3C) return emu->tim_ccr[t * 4 + 2];
    if (r == 0x40) return emu->tim_ccr[t * 4 + 3];
    if (r == 0x44) return emu->tim_bdtr[t];
    return 0;
}

static void timer_com_event(cortex_emu_t *emu, int32_t t) {
    emu->tim_ccmr1_eff[t] = emu->tim_ccmr1[t];
    emu->tim_ccmr2_eff[t] = emu->tim_ccmr2[t];
    emu->tim_ccer_eff[t]  = emu->tim_ccer[t];
    emu->tim_sr[t] |= (1u << 5);  /* COMIF */
}

static int is_ccpc(const cortex_emu_t *emu, int32_t t) {
    return (emu->tim_cr2[t] & 1) != 0;
}

void emu_timer_write(cortex_emu_t *emu, uint32_t addr, uint32_t val) {
    int32_t t = emu_timer_index(addr);
    if (t < 0) return;
    uint32_t r = addr & 0x3FF;
    if (r == 0x00) { emu->tim_cr1[t] = val; return; }
    if (r == 0x04) { emu->tim_cr2[t] = val; return; }
    if (r == 0x08) { emu->tim_smcr[t] = val; return; }
    if (r == 0x0C) { emu->tim_dier[t] = val; return; }
    if (r == 0x10) { emu->tim_sr[t] &= val; return; }  /* rc_w0 */
    if (r == 0x14) {  /* EGR */
        if (val & 1) {  /* UG */
            emu->tim_cnt[t]     = 0;
            emu->tim_pscnt[t]   = 0;
            emu->tim_dir[t]     = 1;
            emu->tim_rcr_cnt[t] = emu->tim_rcr[t] & 0xFF;
            emu->tim_sr[t]     |= 1;
        }
        if (val & (1u << 5)) timer_com_event(emu, t);  /* COMG */
        for (int ch = 0; ch < 4; ch++) {
            if (val & (uint32_t)(1u << (ch + 1)))
                emu->tim_sr[t] |= (uint32_t)(1u << (ch + 1));
        }
        if (val & (1u << 7)) emu->tim_sr[t] |= (1u << 7);  /* BG */
        if (val & (1u << 6)) emu->tim_sr[t] |= (1u << 6);  /* TG */
        return;
    }
    if (r == 0x18) { emu->tim_ccmr1[t] = val; if (!is_ccpc(emu,t)) emu->tim_ccmr1_eff[t]=val; return; }
    if (r == 0x1C) { emu->tim_ccmr2[t] = val; if (!is_ccpc(emu,t)) emu->tim_ccmr2_eff[t]=val; return; }
    if (r == 0x20) { emu->tim_ccer[t]  = val; if (!is_ccpc(emu,t)) emu->tim_ccer_eff[t] =val; return; }
    if (r == 0x24) { emu->tim_cnt[t]  = val; return; }
    if (r == 0x28) { emu->tim_psc[t]  = val; return; }
    if (r == 0x2C) { emu->tim_arr[t]  = val; return; }
    if (r == 0x30) { emu->tim_rcr[t]  = val; return; }
    if (r == 0x34) { emu->tim_ccr[t * 4 + 0] = val; return; }
    if (r == 0x38) { emu->tim_ccr[t * 4 + 1] = val; return; }
    if (r == 0x3C) { emu->tim_ccr[t * 4 + 2] = val; return; }
    if (r == 0x40) { emu->tim_ccr[t * 4 + 3] = val; return; }
    if (r == 0x44) { emu->tim_bdtr[t] = val; return; }
}

void emu_timer_tick(cortex_emu_t *emu, uint32_t ticks) {
    for (int32_t t = 0; t < TIM_N; t++) {
        uint32_t cr1 = emu->tim_cr1[t];
        if (!(cr1 & 1)) continue;  /* CEN not set */

        uint32_t psc = emu->tim_psc[t];
        uint32_t pscnt = emu->tim_pscnt[t];
        uint32_t remaining = ticks;

        while (remaining > 0) {
            uint32_t psc_left = (psc + 1) - pscnt;
            if (remaining < psc_left) {
                pscnt += remaining;
                remaining = 0;
                break;
            }
            remaining -= psc_left;
            pscnt = 0;

            uint32_t arr = emu->tim_arr[t];
            if (arr == 0) break;
            uint32_t cnt = emu->tim_cnt[t];
            uint32_t sr  = emu->tim_sr[t];
            uint32_t cms = (cr1 >> 5) & 3;
            int update_event = 0;

            if (cms != 0) {
                /* Center-aligned */
                int32_t dir = emu->tim_dir[t];
                int was_up = (dir >= 0);
                if (dir >= 0) {
                    cnt++;
                    if (cnt >= arr) {
                        cnt = arr;
                        dir = -1;
                        uint32_t rcr_cnt = emu->tim_rcr_cnt[t];
                        if (rcr_cnt == 0) { update_event = 1; rcr_cnt = emu->tim_rcr[t] & 0xFF; }
                        else rcr_cnt--;
                        emu->tim_rcr_cnt[t] = rcr_cnt;
                    }
                } else {
                    if (cnt > 0) cnt--;
                    if (cnt == 0) {
                        dir = 1;
                        uint32_t rcr_cnt = emu->tim_rcr_cnt[t];
                        if (rcr_cnt == 0) { update_event = 1; rcr_cnt = emu->tim_rcr[t] & 0xFF; }
                        else rcr_cnt--;
                        emu->tim_rcr_cnt[t] = rcr_cnt;
                    }
                }
                emu->tim_dir[t] = dir;
                int can_fire = (cms == 3) || (cms == 1 && !was_up) || (cms == 2 && was_up);
                if (can_fire) {
                    for (int ch = 0; ch < 4; ch++) {
                        if (cnt == emu->tim_ccr[t * 4 + ch])
                            sr |= (uint32_t)(1u << (ch + 1));
                    }
                }
            } else {
                /* Edge-aligned upcounting */
                cnt++;
                if (cnt > arr) {
                    cnt = 0;
                    uint32_t rcr_cnt = emu->tim_rcr_cnt[t];
                    if (rcr_cnt == 0) { update_event = 1; rcr_cnt = emu->tim_rcr[t] & 0xFF; }
                    else rcr_cnt--;
                    emu->tim_rcr_cnt[t] = rcr_cnt;
                }
                for (int ch = 0; ch < 4; ch++) {
                    if (cnt == emu->tim_ccr[t * 4 + ch])
                        sr |= (uint32_t)(1u << (ch + 1));
                }
            }

            if (update_event) {
                sr |= 1;
                if (is_ccpc(emu, t) && (emu->tim_cr2[t] & (1u << 2)))
                    timer_com_event(emu, t);
            }

            emu->tim_cnt[t] = cnt;
            emu->tim_sr[t]  = sr;

            uint32_t dier    = emu->tim_dier[t];
            uint32_t pending = sr & dier;
            if (pending != 0) {
                if (emu->core_type >= CORTEX_EMU_CORE_M3) {
                    /* STM32F103 IRQ mapping */
                    if (t == 0) {
                        if (pending & 1)    emu->nvic_pending |= (1u << 25);
                        if (pending & 0x1E) emu->nvic_pending |= (1u << 27);
                    } else if (t == 1) {
                        emu->nvic_pending |= (1u << 29);
                    } else if (t == 3) {
                        emu->nvic_pending |= (1u << 28);
                    } else if (t == 4) {
                        emu->nvic_pending |= (1u << 30);
                    }
                } else {
                    /* Cortex-M0 / MM32F0020 IRQ mapping */
                    if (t == 0) {
                        if (pending & 0xE1u) emu->nvic_pending |= (1u << 13);
                        if (pending & 0x1Eu) emu->nvic_pending |= (1u << 14);
                    } else if (t == 1) {
                        emu->nvic_pending |= (1u << 16);
                    } else if (t == 5) {
                        emu->nvic_pending |= (1u << 19);
                    } else if (t == 2) {
                        emu->nvic_pending |= (1u << 20);
                    }
                }
            }
        }
        emu->tim_pscnt[t] = pscnt;
    }
}

void emu_timer_reset(cortex_emu_t *emu) {
    for (int32_t t = 0; t < TIM_N; t++) {
        emu->tim_cr1[t] = 0;  emu->tim_cr2[t] = 0;
        emu->tim_smcr[t] = 0; emu->tim_dier[t] = 0;
        emu->tim_sr[t] = 0;   emu->tim_ccmr1[t] = 0;
        emu->tim_ccmr2[t] = 0; emu->tim_ccer[t] = 0;
        emu->tim_cnt[t] = 0;  emu->tim_psc[t] = 0;
        emu->tim_arr[t] = 0;  emu->tim_rcr[t] = 0;
        emu->tim_bdtr[t] = 0; emu->tim_pscnt[t] = 0;
        emu->tim_dir[t] = 1;
        emu->tim_ccmr1_eff[t] = 0;
        emu->tim_ccmr2_eff[t] = 0;
        emu->tim_ccer_eff[t] = 0;
        emu->tim_rcr_cnt[t] = 0;
        for (int ch = 0; ch < 4; ch++) emu->tim_ccr[t * 4 + ch] = 0;
    }
}
