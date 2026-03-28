/*
 * cortex-emu — Thumb-16 instruction decoder
 *
 * Translates executeThumb16() and executeMisc() from AssemblyScript source.
 */

#include "cortex_emu_internal.h"

static uint32_t exec_misc(cortex_emu_t *emu, uint32_t hw);

uint32_t emu_exec_thumb16(cortex_emu_t *emu, uint32_t hw) {
    uint32_t pc = emu->regs[15];

    /* ── Shift by immediate (LSL, LSR, ASR) — 000xx ─────────────────── */
    uint32_t top3 = hw >> 13;
    if (top3 == 0) {
        uint32_t op = (hw >> 11) & 3;
        if (op == 3) {
            /* ADD/SUB register/imm3 */
            uint32_t isImm = (hw >> 10) & 1;
            uint32_t isSub = (hw >> 9) & 1;
            uint32_t rm_imm = (hw >> 6) & 7;
            uint32_t rn = (hw >> 3) & 7;
            uint32_t rd = hw & 7;
            uint32_t rnVal = emu->regs[rn];
            uint32_t operand = isImm ? rm_imm : emu->regs[rm_imm];
            uint32_t result = isSub ? addWithCarry(emu, rnVal, ~operand, 1)
                                    : addWithCarry(emu, rnVal, operand, 0);
            emu->regs[rd] = result;
            return 1;
        }
        uint32_t imm5 = (hw >> 6) & 0x1F;
        uint32_t rm = (hw >> 3) & 7;
        uint32_t rd = hw & 7;
        uint32_t rmVal = emu->regs[rm];
        uint32_t result;
        int carry = flagC(emu);
        if (op == 0) {
            if (imm5 == 0) result = rmVal;
            else { carry = ((rmVal >> (32 - imm5)) & 1) != 0; result = rmVal << imm5; }
        } else if (op == 1) {
            if (imm5 == 0) { carry = (rmVal >> 31) != 0; result = 0; }
            else { carry = ((rmVal >> (imm5 - 1)) & 1) != 0; result = rmVal >> imm5; }
        } else { /* op == 2: ASR */
            if (imm5 == 0) { carry = (rmVal >> 31) != 0; result = carry ? 0xFFFFFFFFu : 0; }
            else { carry = (((int32_t)rmVal >> (imm5 - 1)) & 1) != 0; result = (uint32_t)((int32_t)rmVal >> imm5); }
        }
        emu->regs[rd] = result;
        setNZC(emu, result, carry);
        return 1;
    }

    /* ── MOV/CMP/ADD/SUB immediate — 001xx ───────────────────────────── */
    if (top3 == 1) {
        uint32_t op = (hw >> 11) & 3;
        uint32_t rd = (hw >> 8) & 7;
        uint32_t imm8 = hw & 0xFF;
        if (op == 0) { emu->regs[rd] = imm8; setNZ(emu, imm8); }
        else if (op == 1) { addWithCarry(emu, emu->regs[rd], ~imm8, 1); }
        else if (op == 2) { emu->regs[rd] = addWithCarry(emu, emu->regs[rd], imm8, 0); }
        else { emu->regs[rd] = addWithCarry(emu, emu->regs[rd], ~imm8, 1); }
        return 1;
    }

    /* ── Data processing (register) — 010000 ─────────────────────────── */
    if ((hw >> 10) == 0x10) { /* 0b010000 */
        uint32_t op = (hw >> 6) & 0xF;
        uint32_t rm = (hw >> 3) & 7;
        uint32_t rd = hw & 7;
        uint32_t rdVal = emu->regs[rd];
        uint32_t rmVal = emu->regs[rm];
        uint32_t result = 0;
        if (op == 0) { result = rdVal & rmVal; setNZ(emu, result); emu->regs[rd] = result; }
        else if (op == 1) { result = rdVal ^ rmVal; setNZ(emu, result); emu->regs[rd] = result; }
        else if (op == 2) { /* LSL register */
            uint32_t shift = rmVal & 0xFF;
            if (shift == 0) result = rdVal;
            else if (shift < 32) { result = rdVal << shift; setNZC(emu, result, ((rdVal >> (32 - shift)) & 1) != 0); }
            else { result = 0; setNZC(emu, 0, shift == 32 ? (rdVal & 1) != 0 : 0); }
            emu->regs[rd] = result;
        }
        else if (op == 3) { /* LSR register */
            uint32_t shift = rmVal & 0xFF;
            if (shift == 0) result = rdVal;
            else if (shift < 32) { result = rdVal >> shift; setNZC(emu, result, ((rdVal >> (shift - 1)) & 1) != 0); }
            else { result = 0; setNZC(emu, 0, shift == 32 ? (rdVal >> 31) != 0 : 0); }
            emu->regs[rd] = result;
        }
        else if (op == 4) { /* ASR register */
            uint32_t shift = rmVal & 0xFF;
            if (shift == 0) result = rdVal;
            else if (shift < 32) { result = (uint32_t)((int32_t)rdVal >> shift); setNZC(emu, result, (((int32_t)rdVal >> (shift - 1)) & 1) != 0); }
            else { result = (rdVal & 0x80000000u) ? 0xFFFFFFFFu : 0; setNZC(emu, result, (result & 1) != 0); }
            emu->regs[rd] = result;
        }
        else if (op == 5) { result = addWithCarry(emu, rdVal, rmVal, flagC(emu) ? 1 : 0); emu->regs[rd] = result; }
        else if (op == 6) { result = addWithCarry(emu, rdVal, ~rmVal, flagC(emu) ? 1 : 0); emu->regs[rd] = result; }
        else if (op == 7) { /* ROR register */
            uint32_t shift = rmVal & 0xFF;
            if (shift == 0) result = rdVal;
            else { uint32_t s = shift & 31; result = s == 0 ? rdVal : ((rdVal >> s) | (rdVal << (32 - s))); setNZC(emu, result, (result >> 31) != 0); }
            emu->regs[rd] = result;
        }
        else if (op == 8) { setNZ(emu, rdVal & rmVal); } /* TST */
        else if (op == 9) { result = addWithCarry(emu, 0, ~rmVal, 1); emu->regs[rd] = result; } /* NEG */
        else if (op == 10) { addWithCarry(emu, rdVal, ~rmVal, 1); } /* CMP */
        else if (op == 11) { addWithCarry(emu, rdVal, rmVal, 0); }  /* CMN */
        else if (op == 12) { result = rdVal | rmVal; setNZ(emu, result); emu->regs[rd] = result; }
        else if (op == 13) { result = (uint32_t)((uint64_t)rdVal * (uint64_t)rmVal); setNZ(emu, result); emu->regs[rd] = result; }
        else if (op == 14) { result = rdVal & ~rmVal; setNZ(emu, result); emu->regs[rd] = result; }
        else if (op == 15) { result = ~rmVal; setNZ(emu, result); emu->regs[rd] = result; }
        return 1;
    }

    /* ── Special data / branch exchange — 010001 ─────────────────────── */
    if ((hw >> 10) == 0x11) { /* 0b010001 */
        uint32_t op = (hw >> 8) & 3;
        uint32_t dn = (hw >> 7) & 1;
        uint32_t rm = (hw >> 3) & 0xF;
        uint32_t rd = (hw & 7) | (dn << 3);
        uint32_t rmVal = rm == 15 ? emu->regs[rm] + 2 : emu->regs[rm];
        uint32_t rdVal = rd == 15 ? emu->regs[rd] + 2 : emu->regs[rd];
        if (op == 0) {
            emu->regs[rd] = rdVal + rmVal;
            if (rd == 15) emu->regs[15] &= ~(uint32_t)1;
        } else if (op == 1) {
            addWithCarry(emu, rdVal, ~rmVal, 1);
        } else if (op == 2) {
            emu->regs[rd] = rmVal;
            if (rd == 15) emu->regs[15] &= ~(uint32_t)1;
        } else {
            uint32_t target = rmVal;
            if (hw & 0x80) emu->regs[14] = pc | 1; /* BLX */
            emu->regs[15] = target & ~(uint32_t)1;
            if ((target & 0xFFFFFFF0u) == 0xFFFFFFF0u) emu_exit_exception(emu);
            return 3;
        }
        return 1;
    }

    /* ── LDR literal (PC-relative) — 01001 ───────────────────────────── */
    if ((hw >> 11) == 0x09) { /* 0b01001 */
        uint32_t rd = (hw >> 8) & 7;
        uint32_t imm8 = hw & 0xFF;
        uint32_t addr = ((pc + 2) & ~(uint32_t)3) + (imm8 << 2);
        emu->regs[rd] = cortex_emu_read32(emu, addr);
        return 2;
    }

    /* ── Load/Store register offset — 0101 ───────────────────────────── */
    if ((hw >> 12) == 0x5) { /* 0b0101 */
        uint32_t opc = (hw >> 9) & 7;
        uint32_t rm = (hw >> 6) & 7;
        uint32_t rn = (hw >> 3) & 7;
        uint32_t rd = hw & 7;
        uint32_t addr = emu->regs[rn] + emu->regs[rm];
        if (opc == 0) cortex_emu_write32(emu, addr, emu->regs[rd]);
        else if (opc == 1) cortex_emu_write16(emu, addr, emu->regs[rd]);
        else if (opc == 2) cortex_emu_write8(emu, addr, emu->regs[rd]);
        else if (opc == 3) { uint32_t v = cortex_emu_read8(emu, addr); emu->regs[rd] = (v & 0x80) ? (v | 0xFFFFFF00u) : v; }
        else if (opc == 4) emu->regs[rd] = cortex_emu_read32(emu, addr);
        else if (opc == 5) emu->regs[rd] = cortex_emu_read16(emu, addr);
        else if (opc == 6) emu->regs[rd] = cortex_emu_read8(emu, addr);
        else { uint32_t v = cortex_emu_read16(emu, addr); emu->regs[rd] = (v & 0x8000) ? (v | 0xFFFF0000u) : v; }
        return 2;
    }

    /* ── Load/Store word/byte immediate — 011 ────────────────────────── */
    if ((hw >> 13) == 0x3) { /* 0b011 */
        uint32_t B = (hw >> 12) & 1;
        uint32_t L = (hw >> 11) & 1;
        uint32_t imm5 = (hw >> 6) & 0x1F;
        uint32_t rn = (hw >> 3) & 7;
        uint32_t rd = hw & 7;
        if (B == 0) {
            uint32_t addr = emu->regs[rn] + (imm5 << 2);
            if (L) emu->regs[rd] = cortex_emu_read32(emu, addr);
            else cortex_emu_write32(emu, addr, emu->regs[rd]);
        } else {
            uint32_t addr = emu->regs[rn] + imm5;
            if (L) emu->regs[rd] = cortex_emu_read8(emu, addr);
            else cortex_emu_write8(emu, addr, emu->regs[rd]);
        }
        return 2;
    }

    /* ── Load/Store halfword immediate — 1000 ────────────────────────── */
    if ((hw >> 12) == 0x8) {
        uint32_t L = (hw >> 11) & 1;
        uint32_t imm5 = (hw >> 6) & 0x1F;
        uint32_t rn = (hw >> 3) & 7;
        uint32_t rd = hw & 7;
        uint32_t addr = emu->regs[rn] + (imm5 << 1);
        if (L) emu->regs[rd] = cortex_emu_read16(emu, addr);
        else cortex_emu_write16(emu, addr, emu->regs[rd]);
        return 2;
    }

    /* ── Load/Store SP-relative — 1001 ───────────────────────────────── */
    if ((hw >> 12) == 0x9) {
        uint32_t L = (hw >> 11) & 1;
        uint32_t rd = (hw >> 8) & 7;
        uint32_t imm8 = hw & 0xFF;
        uint32_t addr = emu->regs[13] + (imm8 << 2);
        if (L) emu->regs[rd] = cortex_emu_read32(emu, addr);
        else cortex_emu_write32(emu, addr, emu->regs[rd]);
        return 2;
    }

    /* ── ADD PC/SP + imm — 1010 ──────────────────────────────────────── */
    if ((hw >> 12) == 0xA) {
        uint32_t isSP = (hw >> 11) & 1;
        uint32_t rd = (hw >> 8) & 7;
        uint32_t imm8 = hw & 0xFF;
        if (isSP) emu->regs[rd] = emu->regs[13] + (imm8 << 2);
        else emu->regs[rd] = ((pc + 2) & ~(uint32_t)3) + (imm8 << 2);
        return 1;
    }

    /* ── Miscellaneous — 1011 ────────────────────────────────────────── */
    if ((hw >> 12) == 0xB) return exec_misc(emu, hw);

    /* ── STM/LDM — 1100 ─────────────────────────────────────────────── */
    if ((hw >> 12) == 0xC) {
        uint32_t L = (hw >> 11) & 1;
        uint32_t rn = (hw >> 8) & 7;
        uint32_t regList = hw & 0xFF;
        uint32_t addr = emu->regs[rn];
        for (uint32_t i = 0; i < 8; i++) {
            if (regList & (1u << i)) {
                if (L) emu->regs[i] = cortex_emu_read32(emu, addr);
                else cortex_emu_write32(emu, addr, emu->regs[i]);
                addr += 4;
            }
        }
        if (!L || !(regList & (1u << rn))) emu->regs[rn] = addr;
        return 2;
    }

    /* ── Conditional branch — 1101 ───────────────────────────────────── */
    if ((hw >> 12) == 0xD) {
        uint32_t cond = (hw >> 8) & 0xF;
        if (cond == 0xE) return emu_hard_fault(emu, CORTEX_EMU_FAULT_BAD_COND);
        if (cond == 0xF) return 3; /* SVC */
        if (emu_evaluate_condition(emu, cond)) {
            int32_t offset = (int32_t)(hw & 0xFF);
            if (offset & 0x80) offset |= (int32_t)0xFFFFFF00;
            emu->regs[15] = (uint32_t)((int32_t)(pc + 2) + (offset << 1));
            return 3;
        }
        return 1;
    }

    /* ── Unconditional branch — 11100 ────────────────────────────────── */
    if ((hw >> 11) == 0x1C) { /* 0b11100 */
        int32_t offset = (int32_t)(hw & 0x7FF);
        if (offset & 0x400) offset |= (int32_t)0xFFFFF800;
        emu->regs[15] = (uint32_t)((int32_t)(pc + 2) + (offset << 1));
        return 3;
    }

    return emu_hard_fault(emu, CORTEX_EMU_FAULT_UNDEFINED_INSN);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Miscellaneous 16-bit instructions (1011xxxx)
 * ═══════════════════════════════════════════════════════════════════════ */

static uint32_t exec_misc(cortex_emu_t *emu, uint32_t hw) {
    uint32_t subop = (hw >> 8) & 0xF;

    /* SP adjust */
    if (subop == 0) {
        uint32_t imm7 = (hw & 0x7F) << 2;
        if (hw & 0x80) emu->regs[13] -= imm7;
        else emu->regs[13] += imm7;
        return 1;
    }

    /* SXTH/SXTB/UXTH/UXTB */
    if (subop == 2) {
        uint32_t op = (hw >> 6) & 3;
        uint32_t rm = (hw >> 3) & 7;
        uint32_t rd = hw & 7;
        uint32_t v = emu->regs[rm];
        if (op == 0) emu->regs[rd] = (v & 0x8000) ? (v | 0xFFFF0000u) : (v & 0xFFFF);
        else if (op == 1) emu->regs[rd] = (v & 0x80) ? (v | 0xFFFFFF00u) : (v & 0xFF);
        else if (op == 2) emu->regs[rd] = v & 0xFFFF;
        else emu->regs[rd] = v & 0xFF;
        return 1;
    }

    /* PUSH */
    if (subop == 4 || subop == 5) {
        uint32_t sp = emu->regs[13];
        uint32_t regList = hw & 0xFF;
        int pushLR = (hw & 0x100) != 0;
        uint32_t count = 0;
        for (uint32_t i = 0; i < 8; i++) { if (regList & (1u << i)) count++; }
        if (pushLR) count++;
        sp -= count * 4;
        emu->regs[13] = sp;
        uint32_t addr = sp;
        for (uint32_t i = 0; i < 8; i++) {
            if (regList & (1u << i)) { cortex_emu_write32(emu, addr, emu->regs[i]); addr += 4; }
        }
        if (pushLR) cortex_emu_write32(emu, addr, emu->regs[14]);
        emu_record_shadow_stack(emu, emu->regs[15] - 2, sp);
        if (emu_check_stack_overflow(emu, sp)) return emu_hard_fault(emu, CORTEX_EMU_FAULT_STACK_OVERFLOW);
        return 2;
    }

    /* POP */
    if (subop == 0xC || subop == 0xD) {
        uint32_t addr = emu->regs[13];
        uint32_t regList = hw & 0xFF;
        int popPC = (hw & 0x100) != 0;
        for (uint32_t i = 0; i < 8; i++) {
            if (regList & (1u << i)) { emu->regs[i] = cortex_emu_read32(emu, addr); addr += 4; }
        }
        if (popPC) {
            uint32_t newPC = cortex_emu_read32(emu, addr);
            addr += 4;
            if ((newPC & 0xFFFFFFF0u) == 0xFFFFFFF0u) {
                emu->regs[13] = addr;
                emu_exit_exception(emu);
                return 4;
            }
            emu->regs[15] = newPC & ~(uint32_t)1;
        }
        emu->regs[13] = addr;
        return popPC ? 4 : 2;
    }

    /* BKPT */
    if (subop == 0xE) { emu->breakpoint_hit = 1; return 1; }

    /* IT / hints */
    if (subop == 0xF) {
        uint32_t mask = hw & 0xF;
        if (mask != 0) {
            /* IT instruction */
            uint32_t firstcond = (hw >> 4) & 0xF;
            emu->it_state = (firstcond << 4) | mask;
            return 1;
        }
        /* NOP / WFI / WFE / SEV / YIELD */
        uint32_t hint = (hw >> 4) & 0xF;
        if (hint == 2 || hint == 3) emu->sleeping = 1;
        return 1;
    }

    /* CBZ/CBNZ */
    if (subop == 1 || subop == 3 || subop == 9 || subop == 0xB) {
        uint32_t rn = hw & 7;
        uint32_t i_bit = (hw >> 9) & 1;
        uint32_t imm5 = (hw >> 3) & 0x1F;
        uint32_t offset = (i_bit << 6) | (imm5 << 1);
        int nonzero = subop >= 8;
        uint32_t rnVal = emu->regs[rn];
        if (nonzero ? (rnVal != 0) : (rnVal == 0)) {
            emu->regs[15] = emu->regs[15] + 2 + offset;
        }
        return 1;
    }

    /* CPS */
    if (subop == 6) { if (hw & 0x10) emu->primask = 1; else emu->primask = 0; return 1; }

    /* REV/REV16/REVSH */
    if (subop == 0xA) {
        uint32_t op = (hw >> 6) & 3;
        uint32_t rm = (hw >> 3) & 7;
        uint32_t rd = hw & 7;
        uint32_t v = emu->regs[rm];
        if (op == 0) emu->regs[rd] = ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) | ((v >> 8) & 0xFF00) | ((v >> 24) & 0xFF);
        else if (op == 1) emu->regs[rd] = ((v & 0xFF) << 8) | ((v >> 8) & 0xFF) | ((v & 0xFF0000) << 8) | ((v >> 8) & 0xFF0000);
        else if (op == 3) { uint32_t lo = ((v & 0xFF) << 8) | ((v >> 8) & 0xFF); emu->regs[rd] = (lo & 0x8000) ? (lo | 0xFFFF0000u) : lo; }
        return 1;
    }

    return emu_hard_fault(emu, CORTEX_EMU_FAULT_BAD_MISC);
}
