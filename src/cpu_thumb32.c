#include "cortex_emu_internal.h"
#include <stdint.h>

static int u32_clz(uint32_t val) {
    if (val == 0) return 32;
    int n = 0;
    if ((val & 0xFFFF0000u) == 0) { n += 16; val <<= 16; }
    if ((val & 0xFF000000u) == 0) { n +=  8; val <<=  8; }
    if ((val & 0xF0000000u) == 0) { n +=  4; val <<=  4; }
    if ((val & 0xC0000000u) == 0) { n +=  2; val <<=  2; }
    if ((val & 0x80000000u) == 0) { n +=  1; }
    return n;
}

/* Handle a load into PC: check for EXC_RETURN magic, strip thumb bit */
static uint32_t ldr_to_pc(cortex_emu_t *emu) {
    uint32_t newpc = emu->regs[15];
    if ((newpc & 0xFFFFFFF0u) == 0xFFFFFFF0u) {
        emu_exit_exception(emu);
        return 4;
    }
    emu->regs[15] = newpc & ~1u;
    return 2;
}

uint32_t emu_exec_thumb32(cortex_emu_t *emu, uint32_t hw1, uint32_t hw2) {
    uint32_t pc = emu->regs[15];

    if (hw1 == 0xFFFF && hw2 == 0xFFFF) return 1;

    /* BL / B.W — hw1[15:11] == 0b11110 */
    if ((hw1 >> 11) == 0x1Eu) {
        uint32_t hw2_14 = (hw2 >> 14) & 3;
        uint32_t hw2_12 = (hw2 >> 12) & 1;

        if (hw2_14 == 3 && hw2_12 == 1) {
            /* BL */
            uint32_t S     = (hw1 >> 10) & 1;
            uint32_t imm10 = hw1 & 0x3FF;
            uint32_t J1    = (hw2 >> 13) & 1;
            uint32_t J2    = (hw2 >> 11) & 1;
            uint32_t imm11 = hw2 & 0x7FF;
            uint32_t I1    = (~(J1 ^ S)) & 1;
            uint32_t I2    = (~(J2 ^ S)) & 1;
            int32_t offset = (int32_t)((S << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1));
            if (S) offset |= (int32_t)0xFE000000;
            emu->regs[14] = pc | 1u;
            emu->regs[15] = (uint32_t)((int32_t)pc + offset);
            return 4;
        }
        if (hw2_14 == 2 && hw2_12 == 1) {
            /* B.W unconditional T4 */
            uint32_t S     = (hw1 >> 10) & 1;
            uint32_t imm10 = hw1 & 0x3FF;
            uint32_t J1    = (hw2 >> 13) & 1;
            uint32_t J2    = (hw2 >> 11) & 1;
            uint32_t imm11 = hw2 & 0x7FF;
            uint32_t I1    = (~(J1 ^ S)) & 1;
            uint32_t I2    = (~(J2 ^ S)) & 1;
            int32_t offset = (int32_t)((S << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1));
            if (S) offset |= (int32_t)0xFE000000;
            emu->regs[15] = (uint32_t)((int32_t)pc + offset);
            return 3;
        }
        if (hw2_14 == 2 && hw2_12 == 0) {
            /* B<c>.W conditional T3 */
            uint32_t cond = (hw1 >> 6) & 0xF;
            if (cond < 14) {
                if (emu_evaluate_condition(emu, cond)) {
                    uint32_t S     = (hw1 >> 10) & 1;
                    uint32_t imm6  = hw1 & 0x3F;
                    uint32_t J1    = (hw2 >> 13) & 1;
                    uint32_t J2    = (hw2 >> 11) & 1;
                    uint32_t imm11 = hw2 & 0x7FF;
                    int32_t offset = (int32_t)((S << 20) | (J2 << 19) | (J1 << 18) | (imm6 << 12) | (imm11 << 1));
                    if (S) offset |= (int32_t)0xFFE00000;
                    emu->regs[15] = (uint32_t)((int32_t)pc + offset);
                }
                return 3;
            }
        }

        /* Data Processing (Modified Immediate) */
        if ((hw1 & 0x0200u) == 0 && (hw2 & 0x8000u) == 0) {
            uint32_t i_bit = (hw1 >> 10) & 1;
            uint32_t op    = (hw1 >> 5) & 0xF;
            uint32_t S     = (hw1 >> 4) & 1;
            uint32_t rn    = hw1 & 0xF;
            uint32_t imm3  = (hw2 >> 12) & 7;
            uint32_t rd    = (hw2 >> 8) & 0xF;
            uint32_t imm8  = hw2 & 0xFF;
            uint32_t imm12 = (i_bit << 11) | (imm3 << 8) | imm8;
            uint32_t imm;
            int carry = flagC(emu);
            uint32_t ctrl = imm12 >> 8;
            uint32_t bval = imm12 & 0xFF;
            if (ctrl < 4) {
                if (ctrl == 0)      imm = bval;
                else if (ctrl == 1) imm = bval | (bval << 16);
                else if (ctrl == 2) imm = (bval << 8) | (bval << 24);
                else                imm = bval | (bval << 8) | (bval << 16) | (bval << 24);
            } else {
                uint32_t unrot = 0x80u | (imm12 & 0x7Fu);
                uint32_t amount = (imm12 >> 7) & 0x1F;
                imm = (unrot >> amount) | (unrot << (32 - amount));
                carry = (imm >> 31) != 0;
            }
            uint32_t rn_val = (rn == 15) ? 0 : emu->regs[rn];
            uint32_t result = 0;
            if (op == 0) {
                result = rn_val & imm;
                if (rd != 15) emu->regs[rd] = result;
                if (S) setNZC(emu, result, carry);
            } else if (op == 1) {
                result = rn_val & ~imm;
                if (rd != 15) emu->regs[rd] = result;
                if (S) setNZC(emu, result, carry);
            } else if (op == 2) {
                result = (rn == 15) ? imm : (rn_val | imm);
                if (rd != 15) emu->regs[rd] = result;
                if (S) setNZC(emu, result, carry);
            } else if (op == 3) {
                result = (rn == 15) ? ~imm : (rn_val | ~imm);
                if (rd != 15) emu->regs[rd] = result;
                if (S) setNZC(emu, result, carry);
            } else if (op == 4) {
                result = rn_val ^ imm;
                if (rd != 15) emu->regs[rd] = result;
                if (S) setNZC(emu, result, carry);
            } else if (op == 8) {
                result = S ? addWithCarry(emu, rn_val, imm, 0) : addNoFlags(rn_val, imm, 0);
                if (rd != 15) emu->regs[rd] = result;
            } else if (op == 10) {
                uint32_t c = flagC(emu) ? 1u : 0u;
                result = S ? addWithCarry(emu, rn_val, imm, c) : addNoFlags(rn_val, imm, c);
                if (rd != 15) emu->regs[rd] = result;
            } else if (op == 11) {
                uint32_t c = flagC(emu) ? 1u : 0u;
                result = S ? addWithCarry(emu, rn_val, ~imm, c) : addNoFlags(rn_val, ~imm, c);
                if (rd != 15) emu->regs[rd] = result;
            } else if (op == 13) {
                result = S ? addWithCarry(emu, rn_val, ~imm, 1) : addNoFlags(rn_val, ~imm, 1);
                if (rd != 15) emu->regs[rd] = result;
            } else if (op == 14) {
                result = S ? addWithCarry(emu, ~rn_val, imm, 1) : addNoFlags(~rn_val, imm, 1);
                if (rd != 15) emu->regs[rd] = result;
            }
            return 1;
        }

        /* Data Processing (Plain Binary Immediate) */
        if ((hw1 & 0x0200u) != 0 && (hw2 & 0x8000u) == 0) {
            uint32_t i_bit = (hw1 >> 10) & 1;
            uint32_t op    = (hw1 >> 4) & 0x1F;
            uint32_t rn    = hw1 & 0xF;
            uint32_t imm3  = (hw2 >> 12) & 7;
            uint32_t rd    = (hw2 >> 8) & 0xF;
            uint32_t imm8  = hw2 & 0xFF;
            if (op == 0x00) { /* ADDW */
                uint32_t imm12 = (i_bit << 11) | (imm3 << 8) | imm8;
                emu->regs[rd] = emu->regs[rn] + imm12;
                return 1;
            }
            if (op == 0x04) { /* MOVW */
                uint32_t imm4  = hw1 & 0xF;
                uint32_t imm16 = (imm4 << 12) | (i_bit << 11) | (imm3 << 8) | imm8;
                emu->regs[rd] = imm16;
                return 1;
            }
            if (op == 0x0A) { /* SUBW */
                uint32_t imm12 = (i_bit << 11) | (imm3 << 8) | imm8;
                emu->regs[rd] = emu->regs[rn] - imm12;
                return 1;
            }
            if (op == 0x0C) { /* MOVT */
                uint32_t imm4  = hw1 & 0xF;
                uint32_t imm16 = (imm4 << 12) | (i_bit << 11) | (imm3 << 8) | imm8;
                emu->regs[rd] = (emu->regs[rd] & 0xFFFFu) | (imm16 << 16);
                return 1;
            }
            /* SSAT */
            if ((op & 0x1D) == 0x10) {
                uint32_t sh     = (op >> 1) & 1;
                uint32_t shiftN = (imm3 << 2) | ((hw2 >> 6) & 3);
                uint32_t sat_imm = hw2 & 0x1F;
                int32_t val = (int32_t)emu->regs[rn];
                if (sh) { val = (shiftN == 0) ? (val >> 31) : (val >> shiftN); }
                else { val <<= shiftN; }
                int32_t mx = (int32_t)((1u << sat_imm) - 1);
                int32_t mn = -(int32_t)(1u << sat_imm);
                if (val > mx)      { emu->regs[rd] = (uint32_t)mx; emu->xpsr |= (1u << 27); }
                else if (val < mn) { emu->regs[rd] = (uint32_t)mn; emu->xpsr |= (1u << 27); }
                else                 emu->regs[rd] = (uint32_t)val;
                return 1;
            }
            /* USAT */
            if ((op & 0x1D) == 0x18) {
                uint32_t sh      = (op >> 1) & 1;
                uint32_t shiftN  = (imm3 << 2) | ((hw2 >> 6) & 3);
                uint32_t sat_imm = hw2 & 0x1F;
                int32_t val = (int32_t)emu->regs[rn];
                if (sh) { val = (shiftN == 0) ? (val >> 31) : (val >> shiftN); }
                else { val <<= shiftN; }
                int32_t mx = (int32_t)((1u << sat_imm) - 1);
                if (val > mx)     { emu->regs[rd] = (uint32_t)mx; emu->xpsr |= (1u << 27); }
                else if (val < 0) { emu->regs[rd] = 0; emu->xpsr |= (1u << 27); }
                else                emu->regs[rd] = (uint32_t)val;
                return 1;
            }
            /* SSAT16 */
            if (op == 0x19 && (hw2 & 0xF0E0u) == 0 && emu->core_type >= CORTEX_EMU_CORE_M4) {
                uint32_t sat_imm = (hw2 & 0xF) + 1;
                uint32_t rn_val = emu->regs[rn];
                int32_t mx = (int32_t)((1u << (sat_imm - 1)) - 1);
                int32_t mn = -(int32_t)(1u << (sat_imm - 1));
                int32_t lo = (int32_t)(int16_t)(rn_val & 0xFFFF);
                int32_t hi = (int32_t)(int16_t)((rn_val >> 16) & 0xFFFF);
                if (lo > mx) { lo = mx; emu->xpsr |= (1u<<27); } else if (lo < mn) { lo = mn; emu->xpsr |= (1u<<27); }
                if (hi > mx) { hi = mx; emu->xpsr |= (1u<<27); } else if (hi < mn) { hi = mn; emu->xpsr |= (1u<<27); }
                emu->regs[rd] = ((uint32_t)(lo) & 0xFFFFu) | (((uint32_t)(hi) & 0xFFFFu) << 16);
                return 1;
            }
            /* USAT16 */
            if (op == 0x1D && (hw2 & 0xF0E0u) == 0 && emu->core_type >= CORTEX_EMU_CORE_M4) {
                uint32_t sat_imm = hw2 & 0xF;
                uint32_t rn_val = emu->regs[rn];
                int32_t mx = (int32_t)((1u << sat_imm) - 1);
                int32_t lo = (int32_t)(int16_t)(rn_val & 0xFFFF);
                int32_t hi = (int32_t)(int16_t)((rn_val >> 16) & 0xFFFF);
                if (lo > mx) { lo = mx; emu->xpsr |= (1u<<27); } else if (lo < 0) { lo = 0; emu->xpsr |= (1u<<27); }
                if (hi > mx) { hi = mx; emu->xpsr |= (1u<<27); } else if (hi < 0) { hi = 0; emu->xpsr |= (1u<<27); }
                emu->regs[rd] = ((uint32_t)(lo) & 0xFFFFu) | (((uint32_t)(hi) & 0xFFFFu) << 16);
                return 1;
            }
        }
    } /* end hw1[15:11] == 11110 */

    /* UBFX */
    if ((hw1 & 0xFFF0u) == 0xF3C0u && (hw2 & 0x8000u) == 0) {
        uint32_t rn      = hw1 & 0xF;
        uint32_t imm3    = (hw2 >> 12) & 7;
        uint32_t rd      = (hw2 >> 8) & 0xF;
        uint32_t imm2    = (hw2 >> 6) & 3;
        uint32_t widthm1 = hw2 & 0x1F;
        uint32_t lsb     = (imm3 << 2) | imm2;
        uint32_t width   = widthm1 + 1;
        uint32_t mask    = (width >= 32) ? 0xFFFFFFFFu : ((1u << width) - 1u);
        emu->regs[rd] = (emu->regs[rn] >> lsb) & mask;
        return 1;
    }
    /* SBFX */
    if ((hw1 & 0xFFF0u) == 0xF340u && (hw2 & 0x8000u) == 0) {
        uint32_t rn      = hw1 & 0xF;
        uint32_t imm3    = (hw2 >> 12) & 7;
        uint32_t rd      = (hw2 >> 8) & 0xF;
        uint32_t imm2    = (hw2 >> 6) & 3;
        uint32_t widthm1 = hw2 & 0x1F;
        uint32_t lsb     = (imm3 << 2) | imm2;
        uint32_t width   = widthm1 + 1;
        uint32_t mask    = (width >= 32) ? 0xFFFFFFFFu : ((1u << width) - 1u);
        uint32_t val     = (emu->regs[rn] >> lsb) & mask;
        if (val & (1u << (width - 1))) val |= ~mask;
        emu->regs[rd] = val;
        return 1;
    }
    /* BFI / BFC */
    if ((hw1 & 0xFFF0u) == 0xF360u && (hw2 & 0x8000u) == 0) {
        uint32_t rn    = hw1 & 0xF;
        uint32_t imm3  = (hw2 >> 12) & 7;
        uint32_t rd    = (hw2 >> 8) & 0xF;
        uint32_t imm2  = (hw2 >> 6) & 3;
        uint32_t msb   = hw2 & 0x1F;
        uint32_t lsb   = (imm3 << 2) | imm2;
        uint32_t width = msb - lsb + 1;
        uint32_t mask  = (width >= 32) ? 0xFFFFFFFFu : ((1u << width) - 1u);
        uint32_t rd_val  = emu->regs[rd];
        uint32_t src_val = (rn == 15) ? 0u : emu->regs[rn];
        emu->regs[rd] = (rd_val & ~(mask << lsb)) | ((src_val & mask) << lsb);
        return 1;
    }

    /* MRS */
    if ((hw1 & 0xFFF0u) == 0xF3E0u && (hw2 & 0xF0F0u) == 0x8000u) {
        uint32_t rd     = (hw2 >> 8) & 0xF;
        uint32_t sysreg = hw2 & 0xFF;
        uint32_t val = 0;
        if (sysreg <= 3 || sysreg == 6) val = emu->xpsr;
        else if (sysreg == 5)  val = emu->xpsr & 0x1FF;
        else if (sysreg == 8)  val = emu->regs[13];
        else if (sysreg == 9)  val = emu->psp;
        else if (sysreg == 16) val = emu->primask;
        else if (sysreg == 17) val = emu->basepri;
        else if (sysreg == 19) val = emu->faultmask;
        else if (sysreg == 20) val = emu->control;
        emu->regs[rd] = val;
        return 2;
    }
    /* MSR */
    if ((hw1 & 0xFFF0u) == 0xF380u && (hw2 & 0xFF00u) == 0x8800u) {
        uint32_t rn     = hw1 & 0xF;
        uint32_t sysreg = hw2 & 0xFF;
        uint32_t val    = emu->regs[rn];
        if (sysreg == 0)       emu->xpsr    = (emu->xpsr & 0x07FFFFFFu) | (val & 0xF8000000u);
        else if (sysreg == 8)  emu->regs[13] = val;
        else if (sysreg == 9)  emu->psp      = val;
        else if (sysreg == 16) emu->primask  = val & 1;
        else if (sysreg == 17) emu->basepri  = val & 0xFF;
        else if (sysreg == 19) emu->faultmask = val & 1;
        else if (sysreg == 20) emu->control  = val & 3;
        return 2;
    }
    /* ISB/DSB/DMB/NOP.W/CLREX */
    if ((hw1 & 0xFFE0u) == 0xF3A0u && (hw2 & 0x8000u) != 0) return 1;
    if (hw1 == 0xF3BFu && hw2 == 0x8F2Fu) return 1;

    /* Load/Store Multiple — hw1[15:11] == 0b11101 */
    {
        uint32_t top5 = hw1 >> 11;
        if (top5 == 0x1Du) {
            /* STMIA.W */
            if ((hw1 & 0xFFD0u) == 0xE880u) {
                uint32_t rn = hw1 & 0xF;
                uint32_t W  = (hw1 >> 5) & 1;
                uint32_t addr = emu->regs[rn];
                for (uint32_t i = 0; i < 16; i++) {
                    if (hw2 & (1u << i)) { cortex_emu_write32(emu, addr, emu->regs[i]); addr += 4; }
                }
                if (W) emu->regs[rn] = addr;
                return 2;
            }
            /* LDMIA.W / POP.W */
            if ((hw1 & 0xFFD0u) == 0xE890u) {
                uint32_t rn = hw1 & 0xF;
                uint32_t W  = (hw1 >> 5) & 1;
                uint32_t addr = emu->regs[rn];
                for (uint32_t i = 0; i < 16; i++) {
                    if (hw2 & (1u << i)) { emu->regs[i] = cortex_emu_read32(emu, addr); addr += 4; }
                }
                if (W && !(hw2 & (1u << rn))) emu->regs[rn] = addr;
                if (hw2 & 0x8000u) {
                    uint32_t new_pc = emu->regs[15];
                    if ((new_pc & 0xFFFFFFF0u) == 0xFFFFFFF0u) { emu_exit_exception(emu); return 4; }
                    emu->regs[15] = new_pc & ~1u;
                }
                return 2;
            }
            /* STMDB / PUSH.W */
            if ((hw1 & 0xFFD0u) == 0xE900u) {
                uint32_t rn = hw1 & 0xF;
                uint32_t W  = (hw1 >> 5) & 1;
                uint32_t count = 0;
                for (uint32_t i = 0; i < 16; i++) if (hw2 & (1u << i)) count++;
                uint32_t addr = emu->regs[rn] - count * 4;
                if (W) emu->regs[rn] = addr;
                for (uint32_t i = 0; i < 16; i++) {
                    if (hw2 & (1u << i)) { cortex_emu_write32(emu, addr, emu->regs[i]); addr += 4; }
                }
                return 2;
            }
            /* LDMDB */
            if ((hw1 & 0xFFD0u) == 0xE910u) {
                uint32_t rn = hw1 & 0xF;
                uint32_t W  = (hw1 >> 5) & 1;
                uint32_t count = 0;
                for (uint32_t i = 0; i < 16; i++) if (hw2 & (1u << i)) count++;
                uint32_t addr = emu->regs[rn] - count * 4;
                if (W) emu->regs[rn] = addr;
                for (uint32_t i = 0; i < 16; i++) {
                    if (hw2 & (1u << i)) { emu->regs[i] = cortex_emu_read32(emu, addr); addr += 4; }
                }
                if (hw2 & 0x8000u) {
                    uint32_t new_pc = emu->regs[15];
                    if ((new_pc & 0xFFFFFFF0u) == 0xFFFFFFF0u) { emu_exit_exception(emu); return 4; }
                    emu->regs[15] = new_pc & ~1u;
                }
                return 2;
            }
            /* STREX */
            if ((hw1 & 0xFFF0u) == 0xE840u) {
                uint32_t rn   = hw1 & 0xF;
                uint32_t rt   = (hw2 >> 12) & 0xF;
                uint32_t rd   = (hw2 >> 8) & 0xF;
                uint32_t imm8 = hw2 & 0xFF;
                cortex_emu_write32(emu, emu->regs[rn] + (imm8 << 2), emu->regs[rt]);
                emu->regs[rd] = 0;
                return 2;
            }
            /* LDREX */
            if ((hw1 & 0xFFF0u) == 0xE850u && (hw2 & 0x0F00u) == 0x0F00u) {
                uint32_t rn   = hw1 & 0xF;
                uint32_t rt   = (hw2 >> 12) & 0xF;
                uint32_t imm8 = hw2 & 0xFF;
                emu->regs[rt] = cortex_emu_read32(emu, emu->regs[rn] + (imm8 << 2));
                return 2;
            }
            /* STREXB */
            if ((hw1 & 0xFFF0u) == 0xE8C0u && (hw2 & 0x0FF0u) == 0x0F40u) {
                uint32_t rn = hw1 & 0xF;
                uint32_t rt = (hw2 >> 12) & 0xF;
                uint32_t rd = hw2 & 0xF;
                cortex_emu_write8(emu, emu->regs[rn], emu->regs[rt]);
                emu->regs[rd] = 0;
                return 2;
            }
            /* STREXH */
            if ((hw1 & 0xFFF0u) == 0xE8C0u && (hw2 & 0x0FF0u) == 0x0F50u) {
                uint32_t rn = hw1 & 0xF;
                uint32_t rt = (hw2 >> 12) & 0xF;
                uint32_t rd = hw2 & 0xF;
                cortex_emu_write16(emu, emu->regs[rn], emu->regs[rt]);
                emu->regs[rd] = 0;
                return 2;
            }
            /* LDREXB */
            if ((hw1 & 0xFFF0u) == 0xE8D0u && (hw2 & 0x0FFFu) == 0x0F4Fu) {
                uint32_t rn = hw1 & 0xF;
                uint32_t rt = (hw2 >> 12) & 0xF;
                emu->regs[rt] = cortex_emu_read8(emu, emu->regs[rn]);
                return 2;
            }
            /* LDREXH */
            if ((hw1 & 0xFFF0u) == 0xE8D0u && (hw2 & 0x0FFFu) == 0x0F5Fu) {
                uint32_t rn = hw1 & 0xF;
                uint32_t rt = (hw2 >> 12) & 0xF;
                emu->regs[rt] = cortex_emu_read16(emu, emu->regs[rn]);
                return 2;
            }
            /* TBB / TBH */
            if ((hw1 & 0xFFF0u) == 0xE8D0u && (hw2 & 0xFFE0u) == 0xF000u) {
                uint32_t rn   = hw1 & 0xF;
                uint32_t rm   = hw2 & 0xF;
                uint32_t H    = (hw2 >> 4) & 1;
                uint32_t base = (rn == 15) ? pc : emu->regs[rn];
                uint32_t rm_val = emu->regs[rm];
                if (H) {
                    uint32_t hv = cortex_emu_read16(emu, base + rm_val * 2);
                    emu->regs[15] = pc + hv * 2;
                } else {
                    uint32_t bv = cortex_emu_read8(emu, base + rm_val);
                    emu->regs[15] = pc + bv * 2;
                }
                return 2;
            }
            /* STRD immediate */
            if ((hw1 & 0xFE50u) == 0xE840u) {
                uint32_t P    = (hw1 >> 8) & 1;
                uint32_t U    = (hw1 >> 7) & 1;
                uint32_t W    = (hw1 >> 5) & 1;
                uint32_t rn   = hw1 & 0xF;
                uint32_t rt   = (hw2 >> 12) & 0xF;
                uint32_t rt2  = (hw2 >> 8) & 0xF;
                uint32_t imm8 = hw2 & 0xFF;
                uint32_t offset = imm8 << 2;
                uint32_t addr = emu->regs[rn];
                if (P) addr = U ? addr + offset : addr - offset;
                cortex_emu_write32(emu, addr, emu->regs[rt]);
                cortex_emu_write32(emu, addr + 4, emu->regs[rt2]);
                if (!P) addr = U ? addr + offset : addr - offset;
                if (W || !P) emu->regs[rn] = addr;
                return 2;
            }
            /* LDRD immediate */
            if ((hw1 & 0xFE50u) == 0xE850u) {
                uint32_t P    = (hw1 >> 8) & 1;
                uint32_t U    = (hw1 >> 7) & 1;
                uint32_t W    = (hw1 >> 5) & 1;
                uint32_t rn   = hw1 & 0xF;
                uint32_t rt   = (hw2 >> 12) & 0xF;
                uint32_t rt2  = (hw2 >> 8) & 0xF;
                uint32_t imm8 = hw2 & 0xFF;
                uint32_t offset = imm8 << 2;
                uint32_t addr = (rn == 15) ? (pc & ~3u) : emu->regs[rn];
                if (P) addr = U ? addr + offset : addr - offset;
                emu->regs[rt]  = cortex_emu_read32(emu, addr);
                emu->regs[rt2] = cortex_emu_read32(emu, addr + 4);
                if (!P) addr = U ? addr + offset : addr - offset;
                if (W || !P) emu->regs[rn] = addr;
                return 2;
            }
        } /* end top5 == 0b11101 */

        /* Load/Store Single — hw1[15:11] == 0b11111 */
        if (top5 == 0x1Fu) {
            uint32_t rn = hw1 & 0xF;
            uint32_t rt = (hw2 >> 12) & 0xF;

            /* PLD/PLI hints (Rt=15) — treat as NOP */
            if (rt == 0xF) {
                uint32_t h1m = hw1 & 0xFFF0u;
                if (h1m == 0xF890u || h1m == 0xF810u || h1m == 0xF990u || h1m == 0xF910u ||
                    h1m == 0xF8B0u || h1m == 0xF830u || h1m == 0xF9B0u || h1m == 0xF930u) return 1;
            }
            /* STR.W Rt,[Rn,#imm12] */
            if ((hw1 & 0xFFF0u) == 0xF8C0u) {
                uint32_t imm12 = hw2 & 0xFFF;
                cortex_emu_write32(emu, emu->regs[rn] + imm12, emu->regs[rt]);
                return 2;
            }
            /* LDR.W Rt,[Rn,#imm12] */
            if ((hw1 & 0xFFF0u) == 0xF8D0u) {
                uint32_t imm12 = hw2 & 0xFFF;
                uint32_t addr = ((rn == 15) ? (pc & ~3u) : emu->regs[rn]) + imm12;
                emu->regs[rt] = cortex_emu_read32(emu, addr);
                if (rt == 15) return ldr_to_pc(emu);
                return 2;
            }
            /* STRH.W */
            if ((hw1 & 0xFFF0u) == 0xF8A0u) {
                cortex_emu_write16(emu, emu->regs[rn] + (hw2 & 0xFFF), emu->regs[rt]);
                return 2;
            }
            /* LDRH.W */
            if ((hw1 & 0xFFF0u) == 0xF8B0u) {
                emu->regs[rt] = cortex_emu_read16(emu, emu->regs[rn] + (hw2 & 0xFFF));
                return 2;
            }
            /* STRB.W */
            if ((hw1 & 0xFFF0u) == 0xF880u) {
                cortex_emu_write8(emu, emu->regs[rn] + (hw2 & 0xFFF), emu->regs[rt]);
                return 2;
            }
            /* LDRB.W */
            if ((hw1 & 0xFFF0u) == 0xF890u) {
                emu->regs[rt] = cortex_emu_read8(emu, emu->regs[rn] + (hw2 & 0xFFF));
                return 2;
            }
            /* LDRSB.W */
            if ((hw1 & 0xFFF0u) == 0xF990u) {
                uint32_t v = cortex_emu_read8(emu, emu->regs[rn] + (hw2 & 0xFFF));
                emu->regs[rt] = (v & 0x80) ? (v | 0xFFFFFF00u) : v;
                return 2;
            }
            /* LDRSH.W */
            if ((hw1 & 0xFFF0u) == 0xF9B0u) {
                uint32_t v = cortex_emu_read16(emu, emu->regs[rn] + (hw2 & 0xFFF));
                emu->regs[rt] = (v & 0x8000) ? (v | 0xFFFF0000u) : v;
                return 2;
            }
            /* Register offset variants */
            if ((hw1 & 0xFFF0u) == 0xF840u && (hw2 & 0x0FC0u) == 0) {
                uint32_t rm = hw2 & 0xF; uint32_t sh = (hw2 >> 4) & 3;
                cortex_emu_write32(emu, emu->regs[rn] + (emu->regs[rm] << sh), emu->regs[rt]); return 2;
            }
            if ((hw1 & 0xFFF0u) == 0xF850u && (hw2 & 0x0FC0u) == 0) {
                uint32_t rm = hw2 & 0xF; uint32_t sh = (hw2 >> 4) & 3;
                uint32_t addr = ((rn==15)?(pc&~3u):emu->regs[rn]) + (emu->regs[rm] << sh);
                emu->regs[rt] = cortex_emu_read32(emu, addr);
                if (rt == 15) {
                    if ((emu->regs[15] & 0xFFFFFFF0u) == 0xFFFFFFF0u) {
                        emu_exit_exception(emu);
                        return 4;
                    }
                    emu->regs[15] &= ~1u;
                }
                return 2;
            }
            if ((hw1 & 0xFFF0u) == 0xF820u && (hw2 & 0x0FC0u) == 0) {
            }
            if ((hw1 & 0xFFF0u) == 0xF830u && (hw2 & 0x0FC0u) == 0) {
                uint32_t rm = hw2 & 0xF; uint32_t sh = (hw2 >> 4) & 3;
                emu->regs[rt] = cortex_emu_read16(emu, emu->regs[rn] + (emu->regs[rm] << sh)); return 2;
            }
            if ((hw1 & 0xFFF0u) == 0xF800u && (hw2 & 0x0FC0u) == 0) {
                uint32_t rm = hw2 & 0xF; uint32_t sh = (hw2 >> 4) & 3;
                cortex_emu_write8(emu, emu->regs[rn] + (emu->regs[rm] << sh), emu->regs[rt]); return 2;
            }
            if ((hw1 & 0xFFF0u) == 0xF810u && (hw2 & 0x0FC0u) == 0) {
                uint32_t rm = hw2 & 0xF; uint32_t sh = (hw2 >> 4) & 3;
                emu->regs[rt] = cortex_emu_read8(emu, emu->regs[rn] + (emu->regs[rm] << sh)); return 2;
            }
            if ((hw1 & 0xFFF0u) == 0xF910u && (hw2 & 0x0FC0u) == 0) {
                uint32_t rm = hw2 & 0xF; uint32_t sh = (hw2 >> 4) & 3;
                uint32_t v = cortex_emu_read8(emu, emu->regs[rn] + (emu->regs[rm] << sh));
                emu->regs[rt] = (v & 0x80) ? (v | 0xFFFFFF00u) : v; return 2;
            }
            if ((hw1 & 0xFFF0u) == 0xF930u && (hw2 & 0x0FC0u) == 0) {
                uint32_t rm = hw2 & 0xF; uint32_t sh = (hw2 >> 4) & 3;
                uint32_t v = cortex_emu_read16(emu, emu->regs[rn] + (emu->regs[rm] << sh));
                emu->regs[rt] = (v & 0x8000) ? (v | 0xFFFF0000u) : v; return 2;
            }
            /* Unprivileged variants */
            if ((hw1&0xFFF0u)==0xF840u && (hw2&0x0F00u)==0x0E00u) {
                cortex_emu_write32(emu, emu->regs[rn] + (hw2 & 0xFF), emu->regs[rt]);
                return 2;
            }
            if ((hw1&0xFFF0u)==0xF850u && (hw2&0x0F00u)==0x0E00u) {
                emu->regs[rt] = cortex_emu_read32(emu, emu->regs[rn] + (hw2 & 0xFF));
                if (rt == 15) return ldr_to_pc(emu);
                return 2;
            }
            if ((hw1&0xFFF0u)==0xF800u && (hw2&0x0F00u)==0x0E00u) { cortex_emu_write8(emu,emu->regs[rn]+(hw2&0xFF),emu->regs[rt]); return 2; }
            if ((hw1&0xFFF0u)==0xF810u && (hw2&0x0F00u)==0x0E00u) { emu->regs[rt]=cortex_emu_read8(emu,emu->regs[rn]+(hw2&0xFF)); return 2; }
            if ((hw1&0xFFF0u)==0xF820u && (hw2&0x0F00u)==0x0E00u) { cortex_emu_write16(emu,emu->regs[rn]+(hw2&0xFF),emu->regs[rt]); return 2; }
            if ((hw1&0xFFF0u)==0xF830u && (hw2&0x0F00u)==0x0E00u) { emu->regs[rt]=cortex_emu_read16(emu,emu->regs[rn]+(hw2&0xFF)); return 2; }
            if ((hw1&0xFFF0u)==0xF910u && (hw2&0x0F00u)==0x0E00u) { uint32_t v=cortex_emu_read8(emu,emu->regs[rn]+(hw2&0xFF)); emu->regs[rt]=(v&0x80)?(v|0xFFFFFF00u):v; return 2; }
            if ((hw1&0xFFF0u)==0xF930u && (hw2&0x0F00u)==0x0E00u) { uint32_t v=cortex_emu_read16(emu,emu->regs[rn]+(hw2&0xFF)); emu->regs[rt]=(v&0x8000)?(v|0xFFFF0000u):v; return 2; }
            /* Pre/post-indexed forms */
#define LS32_PP(mask1, read) \
            if ((hw1 & 0xFFF0u) == (mask1) && (hw2 & 0x0800u) != 0) { \
                uint32_t imm8 = hw2 & 0xFF; uint32_t U = (hw2>>9)&1; \
                uint32_t P = (hw2>>10)&1; uint32_t W = (hw2>>8)&1; \
                uint32_t addr = emu->regs[rn]; \
                uint32_t offset = U ? imm8 : (uint32_t)(-(int32_t)imm8); \
                if (P) addr += offset; \
                read; \
                if (!P) addr += offset; \
                if (W || !P) emu->regs[rn] = addr; \
                return 2; \
            }
            LS32_PP(0xF840u, cortex_emu_write32(emu, addr, emu->regs[rt]))
            LS32_PP(0xF850u, { emu->regs[rt]=cortex_emu_read32(emu,addr); if(rt==15) return ldr_to_pc(emu); })
            LS32_PP(0xF800u, cortex_emu_write8(emu, addr, emu->regs[rt]))
            LS32_PP(0xF810u, emu->regs[rt]=cortex_emu_read8(emu,addr))
            LS32_PP(0xF820u, cortex_emu_write16(emu, addr, emu->regs[rt]))
            LS32_PP(0xF830u, emu->regs[rt]=cortex_emu_read16(emu,addr))
            LS32_PP(0xF910u, { uint32_t v=cortex_emu_read8(emu,addr); emu->regs[rt]=(v&0x80)?(v|0xFFFFFF00u):v; })
            LS32_PP(0xF930u, { uint32_t v=cortex_emu_read16(emu,addr); emu->regs[rt]=(v&0x8000)?(v|0xFFFF0000u):v; })
#undef LS32_PP
        } /* end top5 == 0b11111 */

        /* Register-based shifts: LSL.W, LSR.W, ASR.W, ROR.W */
        if ((hw1 & 0xFF80u) == 0xFA00u && (hw2 & 0xF0F0u) == 0xF000u) {
            uint32_t shtype = (hw1 >> 5) & 3;
            uint32_t S      = (hw1 >> 4) & 1;
            uint32_t rn     = hw1 & 0xF;
            uint32_t rd     = (hw2 >> 8) & 0xF;
            uint32_t rm     = hw2 & 0xF;
            uint32_t rn_val = emu->regs[rn];
            uint32_t amount = emu->regs[rm] & 0xFF;
            uint32_t result = 0;
            int carry = flagC(emu);
            if (shtype == 0) {
                if (amount == 0) result = rn_val;
                else if (amount < 32) { result = rn_val << amount; carry = ((rn_val >> (32 - amount)) & 1) != 0; }
                else if (amount == 32) { result = 0; carry = (rn_val & 1) != 0; }
                else { result = 0; carry = 0; }
            } else if (shtype == 1) {
                if (amount == 0) result = rn_val;
                else if (amount < 32) { result = rn_val >> amount; carry = ((rn_val >> (amount - 1)) & 1) != 0; }
                else if (amount == 32) { result = 0; carry = (rn_val >> 31) != 0; }
                else { result = 0; carry = 0; }
            } else if (shtype == 2) {
                if (amount == 0) result = rn_val;
                else if (amount < 32) { result = (uint32_t)((int32_t)rn_val >> amount); carry = ((rn_val >> (amount - 1)) & 1) != 0; }
                else { result = ((rn_val >> 31) != 0) ? 0xFFFFFFFFu : 0u; carry = (rn_val >> 31) != 0; }
            } else {
                if (amount == 0) result = rn_val;
                else { uint32_t a = amount & 31; result = a==0 ? rn_val : ((rn_val>>a)|(rn_val<<(32-a))); carry=(result>>31)!=0; }
            }
            emu->regs[rd] = result;
            if (S) setNZC(emu, result, carry);
            return 1;
        }

        /* Data Processing Shifted Register: hw1[15:9] == 0b1110101 */
        if ((hw1 >> 9) == 0x75u) {
            uint32_t op     = (hw1 >> 5) & 0xF;
            uint32_t S      = (hw1 >> 4) & 1;
            uint32_t rn     = hw1 & 0xF;
            uint32_t imm3   = (hw2 >> 12) & 7;
            uint32_t rd     = (hw2 >> 8) & 0xF;
            uint32_t imm2   = (hw2 >> 6) & 3;
            uint32_t shtype = (hw2 >> 4) & 3;
            uint32_t rm     = hw2 & 0xF;
            uint32_t shiftN = (imm3 << 2) | imm2;
            uint32_t rm_val = emu->regs[rm];
            int carry = flagC(emu);
            if (shtype == 0) {
                if (shiftN > 0) { carry = ((rm_val >> (32-shiftN)) & 1) != 0; rm_val <<= shiftN; }
            } else if (shtype == 1) {
                uint32_t n = (shiftN == 0) ? 32 : shiftN;
                carry = ((rm_val >> (n-1)) & 1) != 0;
                rm_val = (n >= 32) ? 0u : (rm_val >> n);
            } else if (shtype == 2) {
                uint32_t n = (shiftN == 0) ? 32 : shiftN;
                carry = (((int32_t)rm_val >> (int)(n-1)) & 1) != 0;
                rm_val = (n >= 32) ? (uint32_t)((int32_t)rm_val >> 31) : (uint32_t)((int32_t)rm_val >> (int)n);
            } else {
                if (shiftN > 0) { rm_val = (rm_val>>shiftN)|(rm_val<<(32-shiftN)); carry=(rm_val>>31)!=0; }
                else { int prev_carry=carry; carry=(rm_val&1)!=0; rm_val=(prev_carry?0x80000000u:0u)|(rm_val>>1); }
            }
            uint32_t rn_val = (rn == 15) ? 0u : emu->regs[rn];
            uint32_t result = 0;
            if      (op==0) { result=rn_val&rm_val;   if(rd!=15)emu->regs[rd]=result; if(S)setNZC(emu,result,carry); }
            else if (op==1) { result=rn_val&~rm_val;  if(rd!=15)emu->regs[rd]=result; if(S)setNZC(emu,result,carry); }
            else if (op==2) { result=(rn==15)?rm_val:(rn_val|rm_val); emu->regs[rd]=result; if(S)setNZC(emu,result,carry); }
            else if (op==3) { result=(rn==15)?~rm_val:(rn_val|~rm_val); emu->regs[rd]=result; if(S)setNZC(emu,result,carry); }
            else if (op==4) { result=rn_val^rm_val;   if(rd!=15)emu->regs[rd]=result; if(S)setNZC(emu,result,carry); }
            else if (op==6 && emu->core_type>=CORTEX_EMU_CORE_M4) {
                if(shtype<2) emu->regs[rd]=(rn_val&0xFFFFu)|(rm_val&0xFFFF0000u);
                else         emu->regs[rd]=(rn_val&0xFFFF0000u)|(rm_val&0xFFFFu);
            }
            else if (op==8) { result=S?addWithCarry(emu,rn_val,rm_val,0):addNoFlags(rn_val,rm_val,0); if(rd!=15)emu->regs[rd]=result; }
            else if (op==10){ uint32_t c=flagC(emu)?1u:0u; result=S?addWithCarry(emu,rn_val,rm_val,c):addNoFlags(rn_val,rm_val,c); if(rd!=15)emu->regs[rd]=result; }
            else if (op==11){ uint32_t c=flagC(emu)?1u:0u; result=S?addWithCarry(emu,rn_val,~rm_val,c):addNoFlags(rn_val,~rm_val,c); if(rd!=15)emu->regs[rd]=result; }
            else if (op==13){ result=S?addWithCarry(emu,rn_val,~rm_val,1):addNoFlags(rn_val,~rm_val,1); if(rd!=15)emu->regs[rd]=result; }
            else if (op==14){ result=S?addWithCarry(emu,~rn_val,rm_val,1):addNoFlags(~rn_val,rm_val,1); if(rd!=15)emu->regs[rd]=result; }
            return 1;
        }
    }

    /* Multiply/Accumulate */
    if ((hw1 & 0xFFF0u) == 0xFB00u && (hw2 & 0xF0F0u) == 0xF000u) {
        uint32_t rn = hw1 & 0xF, rd = (hw2>>8)&0xF, rm = hw2&0xF;
        emu->regs[rd] = (uint32_t)((uint64_t)emu->regs[rn] * emu->regs[rm] & 0xFFFFFFFFu);
        return 2;
    }
    if ((hw1 & 0xFFF0u) == 0xFB00u && (hw2 & 0x00F0u) == 0x0000u && ((hw2>>12)&0xF) != 0xF) {
        uint32_t rn = hw1 & 0xF, ra = (hw2>>12)&0xF, rd = (hw2>>8)&0xF, rm = hw2&0xF;
        emu->regs[rd] = emu->regs[ra] + (uint32_t)((uint64_t)emu->regs[rn] * emu->regs[rm]);
        return 2;
    }
    if ((hw1 & 0xFFF0u) == 0xFB00u && (hw2 & 0x00F0u) == 0x0010u) {
        uint32_t rn = hw1 & 0xF, ra = (hw2>>12)&0xF, rd = (hw2>>8)&0xF, rm = hw2&0xF;
        emu->regs[rd] = emu->regs[ra] - (uint32_t)((uint64_t)emu->regs[rn] * emu->regs[rm]);
        return 2;
    }
    if ((hw1 & 0xFFF0u) == 0xFB80u && (hw2 & 0x00F0u) == 0x0000u) {
        uint32_t rn=hw1&0xF, rdlo=(hw2>>12)&0xF, rdhi=(hw2>>8)&0xF, rm=hw2&0xF;
        int64_t res = (int64_t)(int32_t)emu->regs[rn] * (int64_t)(int32_t)emu->regs[rm];
        emu->regs[rdlo] = (uint32_t)(res & 0xFFFFFFFF);
        emu->regs[rdhi] = (uint32_t)((res >> 32) & 0xFFFFFFFF);
        return 2;
    }
    if ((hw1 & 0xFFF0u) == 0xFBA0u && (hw2 & 0x00F0u) == 0x0000u) {
        uint32_t rn=hw1&0xF, rdlo=(hw2>>12)&0xF, rdhi=(hw2>>8)&0xF, rm=hw2&0xF;
        uint64_t res = (uint64_t)emu->regs[rn] * emu->regs[rm];
        emu->regs[rdlo] = (uint32_t)(res & 0xFFFFFFFF);
        emu->regs[rdhi] = (uint32_t)((res >> 32) & 0xFFFFFFFF);
        return 2;
    }
    if ((hw1 & 0xFFF0u) == 0xFBC0u && (hw2 & 0x00F0u) == 0x0000u) {
        uint32_t rn=hw1&0xF, rdlo=(hw2>>12)&0xF, rdhi=(hw2>>8)&0xF, rm=hw2&0xF;
        int64_t acc = ((int64_t)(uint64_t)emu->regs[rdhi]<<32)|(int64_t)(uint64_t)emu->regs[rdlo];
        int64_t res = acc + (int64_t)(int32_t)emu->regs[rn] * (int64_t)(int32_t)emu->regs[rm];
        emu->regs[rdlo] = (uint32_t)(res & 0xFFFFFFFF);
        emu->regs[rdhi] = (uint32_t)((res >> 32) & 0xFFFFFFFF);
        return 2;
    }
    if ((hw1 & 0xFFF0u) == 0xFBE0u && (hw2 & 0x00F0u) == 0x0000u) {
        uint32_t rn=hw1&0xF, rdlo=(hw2>>12)&0xF, rdhi=(hw2>>8)&0xF, rm=hw2&0xF;
        uint64_t acc = ((uint64_t)emu->regs[rdhi]<<32)|(uint64_t)emu->regs[rdlo];
        uint64_t res = acc + (uint64_t)emu->regs[rn] * emu->regs[rm];
        emu->regs[rdlo] = (uint32_t)(res & 0xFFFFFFFF);
        emu->regs[rdhi] = (uint32_t)((res >> 32) & 0xFFFFFFFF);
        return 2;
    }
    if ((hw1 & 0xFFF0u) == 0xFBB0u && (hw2 & 0xF0F0u) == 0xF0F0u) {
        uint32_t rn=hw1&0xF, rd=(hw2>>8)&0xF, rm=hw2&0xF;
        uint32_t div = emu->regs[rm];
        emu->regs[rd] = (div == 0) ? 0 : emu->regs[rn] / div;
        return 2;
    }
    if ((hw1 & 0xFFF0u) == 0xFB90u && (hw2 & 0xF0F0u) == 0xF0F0u) {
        uint32_t rn=hw1&0xF, rd=(hw2>>8)&0xF, rm=hw2&0xF;
        int32_t div = (int32_t)emu->regs[rm];
        emu->regs[rd] = (div == 0) ? 0 : (uint32_t)((int32_t)emu->regs[rn] / div);
        return 2;
    }
    if ((hw1 & 0xFFF0u) == 0xFAB0u && (hw2 & 0xF0F0u) == 0xF080u) {
        uint32_t rd=(hw2>>8)&0xF, rm=hw2&0xF;
        emu->regs[rd] = (uint32_t)u32_clz(emu->regs[rm]);
        return 1;
    }
    if ((hw1 & 0xFFF0u) == 0xFA90u && (hw2 & 0xF0F0u) == 0xF0A0u) {
        uint32_t rd=(hw2>>8)&0xF, rm=hw2&0xF;
        uint32_t val=emu->regs[rm], res=0;
        for(int b=0;b<32;b++){res=(res<<1)|(val&1);val>>=1;}
        emu->regs[rd]=res; return 1;
    }
    if ((hw1 & 0xFFF0u) == 0xFA90u && (hw2 & 0xF0F0u) == 0xF080u) {
        uint32_t rd=(hw2>>8)&0xF, rm=hw2&0xF, v=emu->regs[rm];
        emu->regs[rd]=((v&0xFF)<<24)|((v&0xFF00)<<8)|((v>>8)&0xFF00)|((v>>24)&0xFF);
        return 1;
    }
    if ((hw1 & 0xFFF0u) == 0xFA90u && (hw2 & 0xF0F0u) == 0xF090u) {
        uint32_t rd=(hw2>>8)&0xF, rm=hw2&0xF, v=emu->regs[rm];
        emu->regs[rd]=((v&0x00FF)<<8)|((v&0xFF00)>>8)|((v&0x00FF0000)<<8)|((v&0xFF000000)>>8);
        return 1;
    }
    if ((hw1 & 0xFFF0u) == 0xFA90u && (hw2 & 0xF0F0u) == 0xF0B0u) {
        uint32_t rd=(hw2>>8)&0xF, rm=hw2&0xF, v=emu->regs[rm];
        uint32_t half=((v&0xFF)<<8)|((v>>8)&0xFF);
        emu->regs[rd]=(half&0x8000)?(half|0xFFFF0000u):half;
        return 1;
    }
    /* Saturating arith M4: QADD/QDADD/QSUB/QDSUB */
    if ((hw1 & 0xFFF0u) == 0xFA80u && (hw2 & 0xF0C0u) == 0xF080u && emu->core_type >= CORTEX_EMU_CORE_M4) {
        uint32_t rn=hw1&0xF, rd=(hw2>>8)&0xF, rm=hw2&0xF;
        uint32_t op=(hw2>>4)&3;
        int64_t rn_v=(int64_t)(int32_t)emu->regs[rn], rm_v=(int64_t)(int32_t)emu->regs[rm];
        if      (op==0) emu->regs[rd]=signedSat32(emu,rn_v+rm_v);
        else if (op==1) { uint32_t d=signedSat32(emu,rm_v*2); emu->regs[rd]=signedSat32(emu,rn_v+(int64_t)(int32_t)d); }
        else if (op==2) emu->regs[rd]=signedSat32(emu,rn_v-rm_v);
        else            { uint32_t d=signedSat32(emu,rm_v*2); emu->regs[rd]=signedSat32(emu,rn_v-(int64_t)(int32_t)d); }
        return 1;
    }
    /* SEL */
    if ((hw1 & 0xFFF0u) == 0xFAA0u && (hw2 & 0xF0F0u) == 0xF080u && emu->core_type >= CORTEX_EMU_CORE_M4) {
        uint32_t rn=hw1&0xF, rd=(hw2>>8)&0xF, rm=hw2&0xF;
        uint32_t rn_v=emu->regs[rn], rm_v=emu->regs[rm];
        uint32_t ge=(emu->xpsr>>16)&0xF, res=0;
        res|=(ge&1)?(rn_v&0xFF):(rm_v&0xFF);
        res|=(ge&2)?(rn_v&0xFF00):(rm_v&0xFF00);
        res|=(ge&4)?(rn_v&0xFF0000):(rm_v&0xFF0000);
        res|=(ge&8)?(rn_v&0xFF000000u):(rm_v&0xFF000000u);
        emu->regs[rd]=res; return 1;
    }
    /* SXTAH/UXTAH */
    if ((hw1&0xFFF0u)==0xFA00u && (hw2&0xF0C0u)==0xF080u && (hw1&0xF)!=0xF && emu->core_type>=CORTEX_EMU_CORE_M4) {
        uint32_t rn=hw1&0xF, rd=(hw2>>8)&0xF, rm=hw2&0xF, rot=((hw2>>4)&3)<<3;
        uint32_t v=emu->regs[rm], rotv=rot==0?v:((v>>rot)|(v<<(32-rot)));
        emu->regs[rd]=emu->regs[rn]+(uint32_t)(int32_t)(int16_t)(rotv&0xFFFF); return 1;
    }
    if ((hw1&0xFFF0u)==0xFA10u && (hw2&0xF0C0u)==0xF080u && (hw1&0xF)!=0xF && emu->core_type>=CORTEX_EMU_CORE_M4) {
        uint32_t rn=hw1&0xF, rd=(hw2>>8)&0xF, rm=hw2&0xF, rot=((hw2>>4)&3)<<3;
        uint32_t v=emu->regs[rm], rotv=rot==0?v:((v>>rot)|(v<<(32-rot)));
        emu->regs[rd]=emu->regs[rn]+(rotv&0xFFFFu); return 1;
    }

    return emu_hard_fault(emu, CORTEX_EMU_FAULT_UNIMPL_32);
}
