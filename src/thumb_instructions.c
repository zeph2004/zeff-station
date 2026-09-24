#include "thumb_instructions.h"
#include "bus.h"
#include "gba.h"
#include <stdio.h>

static inline uint32_t thumb_add(ARM7TDMI* cpu, uint32_t a, uint32_t b, bool set_flags) {
    uint64_t res64 = (uint64_t)a + (uint64_t)b;
    uint32_t res = (uint32_t)res64;
    if (set_flags) {
        cpu->cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
        if (res & 0x80000000) cpu->cpsr |= FLAG_N;
        if (res == 0) cpu->cpsr |= FLAG_Z;
        if (res64 > 0xFFFFFFFFULL) cpu->cpsr |= FLAG_C;
        if ((~(a ^ b) & (a ^ res)) & 0x80000000) cpu->cpsr |= FLAG_V;
    }
    return res;
}

static inline uint32_t thumb_sub(ARM7TDMI* cpu, uint32_t a, uint32_t b, bool set_flags) {
    uint64_t res64 = (uint64_t)a - (uint64_t)b;
    uint32_t res = (uint32_t)res64;
    if (set_flags) {
        cpu->cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
        if (res & 0x80000000) cpu->cpsr |= FLAG_N;
        if (res == 0) cpu->cpsr |= FLAG_Z;
        if (res64 <= 0xFFFFFFFFULL) cpu->cpsr |= FLAG_C;
        if (((a ^ b) & (a ^ res)) & 0x80000000) cpu->cpsr |= FLAG_V;
    }
    return res;
}

static inline void thumb_set_logic_flags(ARM7TDMI* cpu, uint32_t res) {
    cpu->cpsr &= ~(FLAG_N | FLAG_Z);
    if (res & 0x80000000) cpu->cpsr |= FLAG_N;
    if (res == 0) cpu->cpsr |= FLAG_Z;
}

int thumb_execute_instruction(ARM7TDMI* cpu, uint16_t instr) {
    // Format 1: Move shifted register
    if ((instr & 0xE000) == 0x0000 && (instr & 0x1800) != 0x1800) {
        uint8_t op = (instr >> 11) & 0x3;
        uint8_t offset = (instr >> 6) & 0x1F;
        uint8_t rs = (instr >> 3) & 0x7;
        uint8_t rd = instr & 0x7;
        uint32_t val = cpu->r[rs];
        bool carry = (cpu->cpsr & FLAG_C) != 0;

        if (op == 0) { // LSL
            if (offset == 0) {
                // No shift
            } else {
                carry = (val >> (32 - offset)) & 1;
                val <<= offset;
            }
        } else if (op == 1) { // LSR
            if (offset == 0) offset = 32;
            if (offset == 32) {
                carry = (val >> 31) & 1;
                val = 0;
            } else {
                carry = (val >> (offset - 1)) & 1;
                val >>= offset;
            }
        } else if (op == 2) { // ASR
            if (offset == 0) offset = 32;
            if (offset >= 32) {
                carry = (val >> 31) & 1;
                val = carry ? 0xFFFFFFFF : 0;
            } else {
                carry = ((int32_t)val >> (offset - 1)) & 1;
                val = (uint32_t)((int32_t)val >> offset);
            }
        }

        cpu->r[rd] = val;
        cpu->cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (val & 0x80000000) cpu->cpsr |= FLAG_N;
        if (val == 0) cpu->cpsr |= FLAG_Z;
        if (carry) cpu->cpsr |= FLAG_C;
        return 1;
    }

    // Format 2: Add / Subtract (register or immediate)
    if ((instr & 0xF800) == 0x1800) {
        bool imm = (instr & (1 << 10)) != 0;
        bool sub = (instr & (1 << 9)) != 0;
        uint8_t rn_or_imm = (instr >> 6) & 0x7;
        uint8_t rs = (instr >> 3) & 0x7;
        uint8_t rd = instr & 0x7;
        uint32_t op2 = imm ? rn_or_imm : cpu->r[rn_or_imm];
        uint32_t op1 = cpu->r[rs];

        cpu->r[rd] = sub ? thumb_sub(cpu, op1, op2, true) : thumb_add(cpu, op1, op2, true);
        return 1;
    }

    // Format 3: Move / Compare / Add / Subtract Immediate
    if ((instr & 0xE000) == 0x2000) {
        uint8_t op = (instr >> 11) & 0x3;
        uint8_t rd = (instr >> 8) & 0x7;
        uint32_t offset = instr & 0xFF;

        switch (op) {
            case 0: // MOV
                cpu->r[rd] = offset;
                thumb_set_logic_flags(cpu, offset);
                break;
            case 1: // CMP
                thumb_sub(cpu, cpu->r[rd], offset, true);
                break;
            case 2: // ADD
                cpu->r[rd] = thumb_add(cpu, cpu->r[rd], offset, true);
                break;
            case 3: // SUB
                cpu->r[rd] = thumb_sub(cpu, cpu->r[rd], offset, true);
                break;
        }
        return 1;
    }

    // Format 4: ALU Operations
    if ((instr & 0xFC00) == 0x4000) {
        uint8_t op = (instr >> 6) & 0xF;
        uint8_t rs = (instr >> 3) & 0x7;
        uint8_t rd = instr & 0x7;
        uint32_t val_d = cpu->r[rd];
        uint32_t val_s = cpu->r[rs];
        uint32_t res = 0;

        switch (op) {
            case 0x0: // AND
                res = val_d & val_s;
                cpu->r[rd] = res;
                thumb_set_logic_flags(cpu, res);
                break;
            case 0x1: // EOR
                res = val_d ^ val_s;
                cpu->r[rd] = res;
                thumb_set_logic_flags(cpu, res);
                break;
            case 0x2: { // LSL
                uint8_t shift = val_s & 0xFF;
                bool carry = (cpu->cpsr & FLAG_C) != 0;
                if (shift == 0) {
                    res = val_d;
                } else if (shift < 32) {
                    carry = (val_d >> (32 - shift)) & 1;
                    res = val_d << shift;
                } else if (shift == 32) {
                    carry = val_d & 1;
                    res = 0;
                } else {
                    carry = 0;
                    res = 0;
                }
                cpu->r[rd] = res;
                cpu->cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C);
                if (res & 0x80000000) cpu->cpsr |= FLAG_N;
                if (res == 0) cpu->cpsr |= FLAG_Z;
                if (carry) cpu->cpsr |= FLAG_C;
                break;
            }
            case 0x3: { // LSR
                uint8_t shift = val_s & 0xFF;
                bool carry = (cpu->cpsr & FLAG_C) != 0;
                if (shift == 0) {
                    res = val_d;
                } else if (shift < 32) {
                    carry = (val_d >> (shift - 1)) & 1;
                    res = val_d >> shift;
                } else if (shift == 32) {
                    carry = (val_d >> 31) & 1;
                    res = 0;
                } else {
                    carry = 0;
                    res = 0;
                }
                cpu->r[rd] = res;
                cpu->cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C);
                if (res & 0x80000000) cpu->cpsr |= FLAG_N;
                if (res == 0) cpu->cpsr |= FLAG_Z;
                if (carry) cpu->cpsr |= FLAG_C;
                break;
            }
            case 0x4: { // ASR
                uint8_t shift = val_s & 0xFF;
                bool carry = (cpu->cpsr & FLAG_C) != 0;
                if (shift == 0) {
                    res = val_d;
                } else if (shift < 32) {
                    carry = ((int32_t)val_d >> (shift - 1)) & 1;
                    res = (uint32_t)((int32_t)val_d >> shift);
                } else {
                    carry = (val_d >> 31) & 1;
                    res = carry ? 0xFFFFFFFF : 0;
                }
                cpu->r[rd] = res;
                cpu->cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C);
                if (res & 0x80000000) cpu->cpsr |= FLAG_N;
                if (res == 0) cpu->cpsr |= FLAG_Z;
                if (carry) cpu->cpsr |= FLAG_C;
                break;
            }
            case 0x5: { // ADC
                uint32_t c_in = (cpu->cpsr & FLAG_C) ? 1 : 0;
                uint64_t sum = (uint64_t)val_d + (uint64_t)val_s + c_in;
                res = (uint32_t)sum;
                cpu->r[rd] = res;
                cpu->cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
                if (res & 0x80000000) cpu->cpsr |= FLAG_N;
                if (res == 0) cpu->cpsr |= FLAG_Z;
                if (sum > 0xFFFFFFFFULL) cpu->cpsr |= FLAG_C;
                if ((~(val_d ^ val_s) & (val_d ^ res)) & 0x80000000) cpu->cpsr |= FLAG_V;
                break;
            }
            case 0x6: { // SBC
                uint32_t c_in = (cpu->cpsr & FLAG_C) ? 1 : 0;
                uint64_t diff = (uint64_t)val_d - (uint64_t)val_s - (c_in ? 0 : 1);
                res = (uint32_t)diff;
                cpu->r[rd] = res;
                cpu->cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
                if (res & 0x80000000) cpu->cpsr |= FLAG_N;
                if (res == 0) cpu->cpsr |= FLAG_Z;
                if (diff <= 0xFFFFFFFFULL) cpu->cpsr |= FLAG_C;
                if (((val_d ^ val_s) & (val_d ^ res)) & 0x80000000) cpu->cpsr |= FLAG_V;
                break;
            }
            case 0x7: { // ROR
                uint8_t shift = val_s & 0xFF;
                bool carry = (cpu->cpsr & FLAG_C) != 0;
                if (shift == 0) {
                    res = val_d;
                } else {
                    shift &= 31;
                    if (shift == 0) {
                        carry = (val_d >> 31) & 1;
                        res = val_d;
                    } else {
                        res = (val_d >> shift) | (val_d << (32 - shift));
                        carry = (res >> 31) & 1;
                    }
                }
                cpu->r[rd] = res;
                cpu->cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C);
                if (res & 0x80000000) cpu->cpsr |= FLAG_N;
                if (res == 0) cpu->cpsr |= FLAG_Z;
                if (carry) cpu->cpsr |= FLAG_C;
                break;
            }
            case 0x8: // TST
                thumb_set_logic_flags(cpu, val_d & val_s);
                break;
            case 0x9: // NEG
                cpu->r[rd] = thumb_sub(cpu, 0, val_s, true);
                break;
            case 0xA: // CMP
                thumb_sub(cpu, val_d, val_s, true);
                break;
            case 0xB: // CMN
                thumb_add(cpu, val_d, val_s, true);
                break;
            case 0xC: // ORR
                res = val_d | val_s;
                cpu->r[rd] = res;
                thumb_set_logic_flags(cpu, res);
                break;
            case 0xD: // MUL
                res = val_d * val_s;
                cpu->r[rd] = res;
                thumb_set_logic_flags(cpu, res);
                break;
            case 0xE: // BIC
                res = val_d & ~val_s;
                cpu->r[rd] = res;
                thumb_set_logic_flags(cpu, res);
                break;
            case 0xF: // MVN
                res = ~val_s;
                cpu->r[rd] = res;
                thumb_set_logic_flags(cpu, res);
                break;
        }
        return 1;
    }

    // Format 5: Hi Register Operations / Branch Exchange
    if ((instr & 0xFC00) == 0x4400) {
        uint8_t op = (instr >> 8) & 0x3;
        bool h1 = (instr & (1 << 7)) != 0;
        bool h2 = (instr & (1 << 6)) != 0;
        uint8_t rs = ((instr >> 3) & 0x7) | (h2 ? 8 : 0);
        uint8_t rd = (instr & 0x7) | (h1 ? 8 : 0);

        if (op == 0) { // ADD
            cpu->r[rd] += cpu->r[rs];
            if (rd == 15) {
                cpu->r[15] &= ~1U;
                arm7tdmi_fill_pipeline_thumb(cpu);
                return 3;
            }
        } else if (op == 1) { // CMP
            thumb_sub(cpu, cpu->r[rd], cpu->r[rs], true);
        } else if (op == 2) { // MOV
            cpu->r[rd] = cpu->r[rs];
            if (rd == 15) {
                cpu->r[15] &= ~1U;
                arm7tdmi_fill_pipeline_thumb(cpu);
                return 3;
            }
        } else if (op == 3) { // BX
            uint32_t target = cpu->r[rs];
            if (target & 1) {
                cpu->cpsr |= FLAG_T;
                cpu->r[15] = target & ~1U;
                arm7tdmi_fill_pipeline_thumb(cpu);
            } else {
                cpu->cpsr &= ~FLAG_T;
                cpu->r[15] = target & ~3U;
                arm7tdmi_fill_pipeline_arm(cpu);
            }
            return 3;
        }
        return 1;
    }

    // Format 6: PC-Relative Load
    if ((instr & 0xF800) == 0x4800) {
        uint8_t rd = (instr >> 8) & 0x7;
        uint32_t offset = (instr & 0xFF) * 4;
        uint32_t addr = (cpu->r[15] & ~3U) + offset;
        cpu->r[rd] = bus_read32(&cpu->gba->bus, addr);
        return 2;
    }

    // Format 7: Load / Store with Register Offset
    if ((instr & 0xF200) == 0x5000) {
        bool load = (instr & (1 << 11)) != 0;
        bool byte = (instr & (1 << 10)) != 0;
        uint8_t ro = (instr >> 6) & 0x7;
        uint8_t rb = (instr >> 3) & 0x7;
        uint8_t rd = instr & 0x7;
        uint32_t addr = cpu->r[rb] + cpu->r[ro];

        if (load) {
            cpu->r[rd] = byte ? bus_read8(&cpu->gba->bus, addr) : bus_read32(&cpu->gba->bus, addr);
        } else {
            if (byte) bus_write8(&cpu->gba->bus, addr, (uint8_t)cpu->r[rd]);
            else bus_write32(&cpu->gba->bus, addr & ~3U, cpu->r[rd]);
        }
        return 2;
    }

    // Format 8: Load / Store Sign-Extended Byte / Halfword
    if ((instr & 0xF200) == 0x5200) {
        bool sign = (instr & (1 << 10)) != 0;
        bool half = (instr & (1 << 11)) != 0;
        uint8_t ro = (instr >> 6) & 0x7;
        uint8_t rb = (instr >> 3) & 0x7;
        uint8_t rd = instr & 0x7;
        uint32_t addr = cpu->r[rb] + cpu->r[ro];

        if (!sign && !half) { // STRH
            bus_write16(&cpu->gba->bus, addr & ~1U, (uint16_t)cpu->r[rd]);
        } else if (!sign && half) { // LDRH
            cpu->r[rd] = bus_read16(&cpu->gba->bus, addr);
        } else if (sign && !half) { // LDSB
            cpu->r[rd] = (uint32_t)(int8_t)bus_read8(&cpu->gba->bus, addr);
        } else { // LDSH
            cpu->r[rd] = (uint32_t)(int16_t)bus_read16(&cpu->gba->bus, addr);
        }
        return 2;
    }

    // Format 9: Load / Store with Immediate Offset
    if ((instr & 0xE000) == 0x6000) {
        bool byte = (instr & (1 << 12)) != 0;
        bool load = (instr & (1 << 11)) != 0;
        uint32_t offset = ((instr >> 6) & 0x1F) * (byte ? 1 : 4);
        uint8_t rb = (instr >> 3) & 0x7;
        uint8_t rd = instr & 0x7;
        uint32_t addr = cpu->r[rb] + offset;

        if (load) {
            cpu->r[rd] = byte ? bus_read8(&cpu->gba->bus, addr) : bus_read32(&cpu->gba->bus, addr);
        } else {
            if (byte) bus_write8(&cpu->gba->bus, addr, (uint8_t)cpu->r[rd]);
            else bus_write32(&cpu->gba->bus, addr & ~3U, cpu->r[rd]);
        }
        return 2;
    }

    // Format 10: Load / Store Halfword
    if ((instr & 0xF000) == 0x8000) {
        bool load = (instr & (1 << 11)) != 0;
        uint32_t offset = ((instr >> 6) & 0x1F) * 2;
        uint8_t rb = (instr >> 3) & 0x7;
        uint8_t rd = instr & 0x7;
        uint32_t addr = cpu->r[rb] + offset;

        if (load) {
            cpu->r[rd] = bus_read16(&cpu->gba->bus, addr);
        } else {
            bus_write16(&cpu->gba->bus, addr & ~1U, (uint16_t)cpu->r[rd]);
        }
        return 2;
    }

    // Format 11: SP-Relative Load / Store
    if ((instr & 0xF000) == 0x9000) {
        bool load = (instr & (1 << 11)) != 0;
        uint8_t rd = (instr >> 8) & 0x7;
        uint32_t offset = (instr & 0xFF) * 4;
        uint32_t addr = cpu->r[13] + offset;

        if (load) {
            cpu->r[rd] = bus_read32(&cpu->gba->bus, addr);
        } else {
            bus_write32(&cpu->gba->bus, addr & ~3U, cpu->r[rd]);
        }
        return 2;
    }

    // Format 12: Load Address (ADD Rd, PC/SP, #imm)
    if ((instr & 0xF000) == 0xA000) {
        bool sp = (instr & (1 << 11)) != 0;
        uint8_t rd = (instr >> 8) & 0x7;
        uint32_t offset = (instr & 0xFF) * 4;
        uint32_t base = sp ? cpu->r[13] : (cpu->r[15] & ~3U);
        cpu->r[rd] = base + offset;
        return 1;
    }

    // Format 13: Add Offset to Stack Pointer
    if ((instr & 0xFF00) == 0xB000) {
        bool negative = (instr & (1 << 7)) != 0;
        uint32_t offset = (instr & 0x7F) * 4;
        if (negative) {
            cpu->r[13] -= offset;
        } else {
            cpu->r[13] += offset;
        }
        return 1;
    }

    // Format 14: Push / Pop Registers
    if ((instr & 0xF600) == 0xB400) {
        bool load = (instr & (1 << 11)) != 0; // 0=PUSH, 1=POP
        bool r_bit = (instr & (1 << 8)) != 0; // PUSH LR / POP PC
        uint8_t rlist = instr & 0xFF;

        int reg_count = 0;
        for (int i = 0; i < 8; i++) {
            if (rlist & (1 << i)) reg_count++;
        }
        if (r_bit) reg_count++;

        if (!load) { // PUSH
            uint32_t addr = cpu->r[13] - (reg_count * 4);
            cpu->r[13] = addr;
            for (int i = 0; i < 8; i++) {
                if (rlist & (1 << i)) {
                    bus_write32(&cpu->gba->bus, addr, cpu->r[i]);
                    addr += 4;
                }
            }
            if (r_bit) {
                bus_write32(&cpu->gba->bus, addr, cpu->r[14]);
            }
            return reg_count + 1;
        } else { // POP
            uint32_t addr = cpu->r[13];
            cpu->r[13] += (reg_count * 4);
            for (int i = 0; i < 8; i++) {
                if (rlist & (1 << i)) {
                    cpu->r[i] = bus_read32(&cpu->gba->bus, addr);
                    addr += 4;
                }
            }
            if (r_bit) {
                cpu->r[15] = bus_read32(&cpu->gba->bus, addr) & ~1U;
                arm7tdmi_fill_pipeline_thumb(cpu);
                return reg_count + 3;
            }
            return reg_count + 1;
        }
    }

    // Format 15: Multiple Load / Store (LDMIA / STMIA)
    if ((instr & 0xF000) == 0xC000) {
        bool load = (instr & (1 << 11)) != 0;
        uint8_t rb = (instr >> 8) & 0x7;
        uint8_t rlist = instr & 0xFF;

        int reg_count = 0;
        for (int i = 0; i < 8; i++) {
            if (rlist & (1 << i)) reg_count++;
        }

        uint32_t addr = cpu->r[rb];
        for (int i = 0; i < 8; i++) {
            if (rlist & (1 << i)) {
                if (load) {
                    cpu->r[i] = bus_read32(&cpu->gba->bus, addr);
                } else {
                    bus_write32(&cpu->gba->bus, addr, cpu->r[i]);
                }
                addr += 4;
            }
        }
        cpu->r[rb] = addr;
        return reg_count + 1;
    }

    // Format 16 & 17: Conditional Branch / SWI
    if ((instr & 0xF000) == 0xD000) {
        uint8_t cond = (instr >> 8) & 0xF;
        if (cond == 0xF) { // SWI
            uint8_t comment = instr & 0xFF;
            arm7tdmi_trigger_swi(cpu, comment);
            return 3;
        }

        if (arm7tdmi_check_condition(cpu, cond)) {
            int32_t offset = (int8_t)(instr & 0xFF);
            offset <<= 1;
            cpu->r[15] = (uint32_t)((int32_t)cpu->r[15] + offset);
            arm7tdmi_fill_pipeline_thumb(cpu);
            return 3;
        }
        return 1;
    }

    // Format 18: Unconditional Branch
    if ((instr & 0xF800) == 0xE000) {
        int32_t offset = (int32_t)(instr & 0x7FF);
        if (offset & 0x400) offset |= 0xFFFFF800; // Sign extend 11-bit
        offset <<= 1;
        cpu->r[15] = (uint32_t)((int32_t)cpu->r[15] + offset);
        arm7tdmi_fill_pipeline_thumb(cpu);
        return 3;
    }

    // Format 19: Long Branch with Link (BL)
    if ((instr & 0xF000) == 0xF000) {
        bool second = (instr & (1 << 11)) != 0;
        uint32_t offset = instr & 0x7FF;

        if (!second) {
            // First instruction
            int32_t s_offset = (int32_t)offset;
            if (s_offset & 0x400) s_offset |= 0xFFFFF800;
            s_offset <<= 12;
            cpu->r[14] = (uint32_t)((int32_t)cpu->r[15] + s_offset);
            return 1;
        } else {
            // Second instruction
            uint32_t target = cpu->r[14] + (offset << 1);
            cpu->r[14] = (cpu->r[15] - 2) | 1;
            cpu->r[15] = target & ~1U;
            arm7tdmi_fill_pipeline_thumb(cpu);
            return 3;
        }
    }

    return 1;
}
