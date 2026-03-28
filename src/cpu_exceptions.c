#include "cortex_emu_internal.h"

int emu_evaluate_condition(const cortex_emu_t *emu, uint32_t cond) {
    switch (cond) {
        case 0:  return flagZ(emu);
        case 1:  return !flagZ(emu);
        case 2:  return flagC(emu);
        case 3:  return !flagC(emu);
        case 4:  return flagN(emu);
        case 5:  return !flagN(emu);
        case 6:  return flagV(emu);
        case 7:  return !flagV(emu);
        case 8:  return flagC(emu) && !flagZ(emu);
        case 9:  return !flagC(emu) || flagZ(emu);
        case 10: return flagN(emu) == flagV(emu);
        case 11: return flagN(emu) != flagV(emu);
        case 12: return !flagZ(emu) && (flagN(emu) == flagV(emu));
        case 13: return flagZ(emu) || (flagN(emu) != flagV(emu));
        default: return 1;
    }
}

uint32_t emu_hard_fault(cortex_emu_t *emu, uint32_t code) {
    emu->hard_fault_count++;
    emu->hard_fault_code = code;
    if (emu->active_exception == 3) {
        emu->lockup = 1;
        emu->halted = 1;
        return 0;
    }
    emu_enter_exception(emu, 3);
    return 3;
}

void emu_set_pending_exception(cortex_emu_t *emu, int32_t exc_num) {
    if (exc_num >= 16 && exc_num < 16 + MAX_IRQ)
        emu->nvic_pending |= (1u << (exc_num - 16));
    else if (exc_num == 15)
        emu->nvic_pending |= 0x80000000u;
}

uint32_t emu_get_irq_priority(const cortex_emu_t *emu, int32_t irqn) {
    if (irqn < 0 || irqn >= MAX_IRQ) return 255;
    int byte_idx = irqn >> 2;
    int shift = (irqn & 3) << 3;
    uint32_t prio = (emu->nvic_ipr[byte_idx] >> shift) & 0xFF;
    if (emu->core_type <= CORTEX_EMU_CORE_M0PLUS) return prio >> 6;
    return prio;
}

uint32_t emu_get_sys_exception_priority(const cortex_emu_t *emu, int32_t exc_num) {
    if (exc_num <= 3) return 0;
    uint32_t byte_val = 0;
    if (exc_num >= 8 && exc_num <= 11)
        byte_val = (emu->scb_shpr2 >> (((uint32_t)(exc_num - 8)) << 3)) & 0xFF;
    else if (exc_num >= 12 && exc_num <= 15)
        byte_val = (emu->scb_shpr3 >> (((uint32_t)(exc_num - 12)) << 3)) & 0xFF;
    if (emu->core_type <= CORTEX_EMU_CORE_M0PLUS) return byte_val >> 6;
    return byte_val;
}

uint32_t emu_get_active_exception_priority(const cortex_emu_t *emu) {
    if (emu->active_exception == 0) return 256;
    if (emu->active_exception < 16)
        return emu_get_sys_exception_priority(emu, emu->active_exception);
    return emu_get_irq_priority(emu, emu->active_exception - 16);
}

void emu_check_pending_interrupts(cortex_emu_t *emu) {
    if (emu->primask & 1) return;
    uint32_t active_prio = emu_get_active_exception_priority(emu);
    /* SysTick */
    if (emu->nvic_pending & 0x80000000u) {
        uint32_t systick_prio = emu_get_sys_exception_priority(emu, 15);
        if (emu->active_exception == 0 || systick_prio < active_prio) {
            emu->nvic_pending &= ~0x80000000u;
            emu_enter_exception(emu, 15);
            return;
        }
    }
    uint32_t pending = emu->nvic_pending & emu->nvic_enabled;
    if (pending == 0) return;
    int best_irq = -1;
    uint32_t best_prio = 256;
    for (int i = 0; i < MAX_IRQ; i++) {
        if (pending & (1u << i)) {
            uint32_t prio = emu_get_irq_priority(emu, i);
            if (prio < best_prio) { best_prio = prio; best_irq = i; }
        }
    }
    if (best_irq >= 0) {
        if (emu->active_exception == 0 || best_prio < active_prio) {
            emu->nvic_pending &= ~(1u << best_irq);
            emu_enter_exception(emu, best_irq + 16);
        }
    }
}

void emu_enter_exception(cortex_emu_t *emu, int32_t exc_num) {
    emu->exception_entry_count++;
    uint32_t sp = emu->regs[13];
    sp -= 32;
    emu_record_shadow_stack(emu, emu->regs[15], sp);
    if (exc_num != 3 && emu_check_stack_overflow(emu, sp)) {
        emu_hard_fault(emu, CORTEX_EMU_FAULT_STACK_OVERFLOW);
        return;
    }
    cortex_emu_write32(emu, sp + 0,  emu->regs[0]);
    cortex_emu_write32(emu, sp + 4,  emu->regs[1]);
    cortex_emu_write32(emu, sp + 8,  emu->regs[2]);
    cortex_emu_write32(emu, sp + 12, emu->regs[3]);
    cortex_emu_write32(emu, sp + 16, emu->regs[12]);
    cortex_emu_write32(emu, sp + 20, emu->regs[14]);
    cortex_emu_write32(emu, sp + 24, emu->regs[15]);
    uint32_t it = emu->it_state;
    uint32_t saved_xpsr = (emu->xpsr & 0xF9FF03FFu)
                        | ((it & 0x03u) << 25)
                        | ((it & 0xFCu) << 8);
    cortex_emu_write32(emu, sp + 28, saved_xpsr);
    emu->regs[13] = sp;
    emu->regs[14] = (emu->active_exception != 0) ? 0xFFFFFFF1u : 0xFFFFFFF9u;
    uint32_t vector_addr = cortex_emu_read32(emu, (uint32_t)(exc_num * 4));
    emu->regs[15] = vector_addr & ~1u;
    emu->active_exception = exc_num;
    emu->xpsr = (emu->xpsr & 0xFFFFFE00u) | (uint32_t)(exc_num & 0x1FF);
    emu->it_state = 0;
}

void emu_exit_exception(cortex_emu_t *emu) {
    emu->exception_exit_count++;
    uint32_t sp = emu->regs[13];
    emu->regs[0]  = cortex_emu_read32(emu, sp + 0);
    emu->regs[1]  = cortex_emu_read32(emu, sp + 4);
    emu->regs[2]  = cortex_emu_read32(emu, sp + 8);
    emu->regs[3]  = cortex_emu_read32(emu, sp + 12);
    emu->regs[12] = cortex_emu_read32(emu, sp + 16);
    emu->regs[14] = cortex_emu_read32(emu, sp + 20);
    emu->regs[15] = cortex_emu_read32(emu, sp + 24);
    uint32_t stacked_xpsr = cortex_emu_read32(emu, sp + 28);
    emu->it_state = (uint8_t)(((stacked_xpsr >> 25) & 0x03u) | ((stacked_xpsr >> 8) & 0xFCu));
    emu->xpsr = stacked_xpsr & 0xF9FF03FFu;
    sp += 32;
    emu->regs[13] = sp;
    emu->active_exception = (int32_t)(emu->xpsr & 0x1FF);
}
