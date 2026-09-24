#include "arm_instructions.h"
#include "gba.h"
#include "bus.h"
#include <stdio.h>

// Barrel Shifter helper functions
static inline uint32_t arm_shift(ARM7TDMI* cpu, uint8_t shift_type, uint32_t val, uint32_t amount, bool* carry_out, bool immediate) {
    bool carry = (cpu->cpsr & FLAG_C) != 0;

    switch (shift_type) {
        case 0: // LSL
            if (amount == 0) {
                // No shift, carry unchanged
            } else if (amount < 32) {
                carry = (val >> (32 - amount)) & 1;
                val <<= amount;
            } else if (amount == 32) {
                carry = val & 1;
                val = 0;
            } else {
                carry = 0;
                val = 0;
            }
            break;

        case 1: // LSR
            if (immediate && amount == 0) amount = 32;
            if (amount == 0) {
                // No shift
            } else if (amount < 32) {
                carry = (val >> (amount - 1)) & 1;
                val >>= amount;
            } else if (amount == 32) {
                carry = (val >> 31) & 1;
                val = 0;
            } else {
                carry = 0;
                val = 0;
            }
            break;

        case 2: // ASR
            if (immediate && amount == 0) amount = 32;
            if (amount == 0) {
                // No shift
            } else if (amount < 32) {
                carry = ((int32_t)val >> (amount - 1)) & 1;
                val = (uint32_t)((int32_t)val >> amount);
            } else {
                bool sign = (val >> 31) & 1;
                carry = sign;
                val = sign ? 0xFFFFFFFF : 0;
            }
            break;

        case 3: // ROR / RRX
            if (immediate && amount == 0) {
                // RRX
                uint32_t old_c = (cpu->cpsr & FLAG_C) ? 1 : 0;
                carry = val & 1;
                val = (val >> 1) | (old_c << 31);
            } else if (amount == 0) {
                // No shift
            } else {
                amount &= 31;
                if (amount == 0) {
                    carry = (val >> 31) & 1;
                } else {
                    val = (val >> amount) | (val << (32 - amount));
                    carry = (val >> 31) & 1;
                }
            }
            break;
    }

    *carry_out = carry;
    return val;
}

// Add with flags helper
static inline uint32_t alu_add(ARM7TDMI* cpu, uint32_t a, uint32_t b, uint32_t c_in, bool set_flags) {
    uint64_t res64 = (uint64_t)a + (uint64_t)b + c_in;
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

// Sub with flags helper
static inline uint32_t alu_sub(ARM7TDMI* cpu, uint32_t a, uint32_t b, uint32_t c_in, bool set_flags) {
    uint64_t res64 = (uint64_t)a - (uint64_t)b - (c_in ? 0 : 1);
    uint32_t res = (uint32_t)res64;
    if (set_flags) {
        cpu->cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
        if (res & 0x80000000) cpu->cpsr |= FLAG_N;
        if (res == 0) cpu->cpsr |= FLAG_Z;
        if (res64 <= 0xFFFFFFFFULL) cpu->cpsr |= FLAG_C; // Not borrow
        if (((a ^ b) & (a ^ res)) & 0x80000000) cpu->cpsr |= FLAG_V;
    }
    return res;
}

// Set logical flags helper
static inline void alu_set_logic_flags(ARM7TDMI* cpu, uint32_t res, bool carry) {
    cpu->cpsr &= ~(FLAG_N | FLAG_Z | FLAG_C);
    if (res & 0x80000000) cpu->cpsr |= FLAG_N;
    if (res == 0) cpu->cpsr |= FLAG_Z;
    if (carry) cpu->cpsr |= FLAG_C;
}

int arm_execute_instruction(ARM7TDMI* cpu, uint32_t instr) {
    // 1. Condition check
    uint8_t cond = (instr >> 28) & 0xF;
    if (!arm7tdmi_check_condition(cpu, cond)) {
        return 1;
    }

    // 2. Branch and Exchange (BX): 0001 0010 .... 0001
    if ((instr & 0x0FFFFFF0) == 0x012FFF10) {
        uint8_t rn = instr & 0xF;
        uint32_t target = cpu->r[rn];
        if (target & 1) {
            // Switch to THUMB
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

    // 3. Branch / Branch with Link (B / BL): 101L offset
    if ((instr & 0x0E000000) == 0x0A000000) {
        bool link = (instr & (1 << 24)) != 0;
        int32_t offset = (int32_t)(instr & 0x00FFFFFF);
        if (offset & 0x00800000) {
            offset |= 0xFF000000; // Sign extend
        }
        offset <<= 2;

        if (link) {
            cpu->r[14] = cpu->r[15] - 4; // Address of next instruction
        }

        cpu->r[15] = (uint32_t)((int32_t)cpu->r[15] + offset);
        arm7tdmi_fill_pipeline_arm(cpu);
        return 3;
    }

    // 4. Software Interrupt (SWI): 1111
    if ((instr & 0x0F000000) == 0x0F000000) {
        uint8_t comment = (uint8_t)(instr >> 16);
        arm7tdmi_trigger_swi(cpu, comment);
        return 3;
    }

    // 5. PSR Transfer (MRS / MSR)
    if ((instr & 0x0FBF0FFF) == 0x010F0000) {
        // MRS
        uint8_t rd = (instr >> 12) & 0xF;
        bool spsr = (instr & (1 << 22)) != 0;
        cpu->r[rd] = spsr ? *arm7tdmi_get_spsr(cpu) : cpu->cpsr;
        return 1;
    }

    if ((instr & 0x0DB0F000) == 0x0120F000) {
        // MSR
        bool imm = (instr & (1 << 25)) != 0;
        bool spsr = (instr & (1 << 22)) != 0;
        uint32_t val;
        if (imm) {
            val = instr & 0xFF;
            uint32_t rot = ((instr >> 8) & 0xF) * 2;
            if (rot) val = (val >> rot) | (val << (32 - rot));
        } else {
            val = cpu->r[instr & 0xF];
        }

        uint32_t mask = 0;
        if (instr & (1 << 19)) mask |= 0xFF000000; // Flags
        if (instr & (1 << 18)) mask |= 0x00FF0000; // Status
        if (instr & (1 << 17)) mask |= 0x0000FF00; // Extension
        if (instr & (1 << 16)) mask |= 0x000000FF; // Control

        // USR mode can only change flag bits
        if ((cpu->cpsr & MODE_MASK) == MODE_USR) {
            mask &= 0xFF000000;
        }

        if (spsr) {
            uint32_t* pspsr = arm7tdmi_get_spsr(cpu);
            *pspsr = (*pspsr & ~mask) | (val & mask);
        } else {
            uint32_t new_cpsr = (cpu->cpsr & ~mask) | (val & mask);
            if ((new_cpsr & MODE_MASK) != (cpu->cpsr & MODE_MASK)) {
                arm7tdmi_switch_mode(cpu, (CpuMode)(new_cpsr & MODE_MASK));
            }
            cpu->cpsr = new_cpsr;
        }
        return 1;
    }

    // 6. Multiply / Multiply Long
    if ((instr & 0x0FC000F0) == 0x00000090) {
        // MUL / MLA
        uint8_t rd = (instr >> 16) & 0xF;
        uint8_t rn = (instr >> 12) & 0xF;
        uint8_t rs = (instr >> 8) & 0xF;
        uint8_t rm = instr & 0xF;
        bool accumulate = (instr & (1 << 21)) != 0;
        bool set_flags = (instr & (1 << 20)) != 0;

        uint32_t res = cpu->r[rm] * cpu->r[rs];
        if (accumulate) res += cpu->r[rn];
        cpu->r[rd] = res;

        if (set_flags) {
            cpu->cpsr &= ~(FLAG_N | FLAG_Z);
            if (res & 0x80000000) cpu->cpsr |= FLAG_N;
            if (res == 0) cpu->cpsr |= FLAG_Z;
        }
        return 2;
    }

    if ((instr & 0x0F8000F0) == 0x00800090) {
        // UMULL, UMLAL, SMULL, SMLAL
        uint8_t rdhi = (instr >> 16) & 0xF;
        uint8_t rdlo = (instr >> 12) & 0xF;
        uint8_t rs = (instr >> 8) & 0xF;
        uint8_t rm = instr & 0xF;
        bool is_signed = (instr & (1 << 22)) != 0;
        bool accumulate = (instr & (1 << 21)) != 0;
        bool set_flags = (instr & (1 << 20)) != 0;

        uint64_t res;
        if (is_signed) {
            int64_t prod = (int64_t)(int32_t)cpu->r[rm] * (int64_t)(int32_t)cpu->r[rs];
            if (accumulate) {
                int64_t acc = ((int64_t)cpu->r[rdhi] << 32) | (uint64_t)cpu->r[rdlo];
                prod += acc;
            }
            res = (uint64_t)prod;
        } else {
            uint64_t prod = (uint64_t)cpu->r[rm] * (uint64_t)cpu->r[rs];
            if (accumulate) {
                uint64_t acc = ((uint64_t)cpu->r[rdhi] << 32) | (uint64_t)cpu->r[rdlo];
                prod += acc;
            }
            res = prod;
        }

        cpu->r[rdlo] = (uint32_t)res;
        cpu->r[rdhi] = (uint32_t)(res >> 32);

        if (set_flags) {
            cpu->cpsr &= ~(FLAG_N | FLAG_Z);
            if (res & 0x8000000000000000ULL) cpu->cpsr |= FLAG_N;
            if (res == 0) cpu->cpsr |= FLAG_Z;
        }
        return 3;
    }

    // 7. Single Data Swap (SWP / SWPB)
    if ((instr & 0x0FB00FF0) == 0x01000090) {
        uint8_t rn = (instr >> 16) & 0xF;
        uint8_t rd = (instr >> 12) & 0xF;
        uint8_t rm = instr & 0xF;
        bool byte = (instr & (1 << 22)) != 0;
        uint32_t addr = cpu->r[rn];

        if (byte) {
            uint8_t data = bus_read8(&cpu->gba->bus, addr);
            bus_write8(&cpu->gba->bus, addr, (uint8_t)cpu->r[rm]);
            cpu->r[rd] = data;
        } else {
            uint32_t data = bus_read32(&cpu->gba->bus, addr);
            bus_write32(&cpu->gba->bus, addr, cpu->r[rm]);
            cpu->r[rd] = data;
        }
        return 4;
    }

    // 8. Halfword and Signed Data Transfer (LDRH, STRH, LDRSB, LDRSH)
    if ((instr & 0x0E000090) == 0x00000090) {
        bool pre = (instr & (1 << 24)) != 0;
        bool up = (instr & (1 << 23)) != 0;
        bool imm = (instr & (1 << 22)) != 0;
        bool writeback = (instr & (1 << 21)) != 0;
        bool load = (instr & (1 << 20)) != 0;
        uint8_t rn = (instr >> 16) & 0xF;
        uint8_t rd = (instr >> 12) & 0xF;
        uint8_t op = ((instr >> 5) & 0x3);

        uint32_t offset = imm ? (((instr >> 8) & 0xF) << 4) | (instr & 0xF) : cpu->r[instr & 0xF];
        uint32_t addr = cpu->r[rn];

        if (pre) {
            addr = up ? (addr + offset) : (addr - offset);
        }

        if (load) {
            if (op == 1) { // LDRH
                cpu->r[rd] = bus_read16(&cpu->gba->bus, addr);
            } else if (op == 2) { // LDRSB
                cpu->r[rd] = (uint32_t)(int8_t)bus_read8(&cpu->gba->bus, addr);
            } else if (op == 3) { // LDRSH
                cpu->r[rd] = (uint32_t)(int16_t)bus_read16(&cpu->gba->bus, addr);
            }
            if (rd == 15) arm7tdmi_fill_pipeline_arm(cpu);
        } else {
            if (op == 1) { // STRH
                bus_write16(&cpu->gba->bus, addr, (uint16_t)cpu->r[rd]);
            }
        }

        if (!pre) {
            addr = up ? (addr + offset) : (addr - offset);
            cpu->r[rn] = addr;
        } else if (writeback) {
            cpu->r[rn] = addr;
        }
        return 2;
    }

    // 9. Block Data Transfer (LDM / STM)
    if ((instr & 0x0E000000) == 0x08000000) {
        bool pre = (instr & (1 << 24)) != 0;
        bool up = (instr & (1 << 23)) != 0;
        bool s_bit = (instr & (1 << 22)) != 0;
        bool writeback = (instr & (1 << 21)) != 0;
        bool load = (instr & (1 << 20)) != 0;
        uint8_t rn = (instr >> 16) & 0xF;
        uint16_t reg_list = instr & 0xFFFF;

        int reg_count = 0;
        for (int i = 0; i < 16; i++) {
            if (reg_list & (1 << i)) reg_count++;
        }

        uint32_t addr = cpu->r[rn];
        if (up) {
            addr = pre ? (addr + 4) : addr;
        } else {
            addr = pre ? (addr - reg_count * 4) : (addr - reg_count * 4 + 4);
        }

        bool pc_loaded = false;
        int cycles = 1;

        for (int i = 0; i < 16; i++) {
            if (reg_list & (1 << i)) {
                if (load) {
                    uint32_t val = bus_read32(&cpu->gba->bus, addr);
                    if (i == 15) {
                        cpu->r[15] = val;
                        pc_loaded = true;
                    } else {
                        cpu->r[i] = val;
                    }
                } else {
                    uint32_t val = cpu->r[i];
                    if (i == 15) val += 4; // STR/STM of PC stores PC + 12 on ARMv4
                    bus_write32(&cpu->gba->bus, addr, val);
                }
                addr += 4;
                cycles++;
            }
        }

        if (writeback && !(load && (reg_list & (1 << rn)))) {
            cpu->r[rn] = up ? (cpu->r[rn] + reg_count * 4) : (cpu->r[rn] - reg_count * 4);
        }

        if (pc_loaded) {
            if (s_bit) {
                uint32_t* pspsr = arm7tdmi_get_spsr(cpu);
                if ((cpu->cpsr & MODE_MASK) != (*pspsr & MODE_MASK)) {
                    arm7tdmi_switch_mode(cpu, (CpuMode)(*pspsr & MODE_MASK));
                }
                cpu->cpsr = *pspsr;
            }
            if (cpu->cpsr & FLAG_T) {
                arm7tdmi_fill_pipeline_thumb(cpu);
            } else {
                arm7tdmi_fill_pipeline_arm(cpu);
            }
            cycles += 2;
        }

        return cycles;
    }

    // 10. Single Data Transfer (LDR / STR / LDRB / STRB)
    if ((instr & 0x0C000000) == 0x04000000) {
        bool imm = (instr & (1 << 25)) == 0;
        bool pre = (instr & (1 << 24)) != 0;
        bool up = (instr & (1 << 23)) != 0;
        bool byte = (instr & (1 << 22)) != 0;
        bool writeback = (instr & (1 << 21)) != 0;
        bool load = (instr & (1 << 20)) != 0;
        uint8_t rn = (instr >> 16) & 0xF;
        uint8_t rd = (instr >> 12) & 0xF;

        uint32_t offset;
        if (imm) {
            offset = instr & 0xFFF;
        } else {
            uint8_t shift_type = (instr >> 5) & 0x3;
            uint8_t shift_amount = (instr >> 7) & 0x1F;
            bool dummy;
            offset = arm_shift(cpu, shift_type, cpu->r[instr & 0xF], shift_amount, &dummy, true);
        }

        uint32_t addr = cpu->r[rn];
        if (pre) {
            addr = up ? (addr + offset) : (addr - offset);
        }

        if (load) {
            if (byte) {
                cpu->r[rd] = bus_read8(&cpu->gba->bus, addr);
            } else {
                // Unaligned LDR: rotate aligned word
                uint32_t val = bus_read32(&cpu->gba->bus, addr & ~3U);
                uint32_t rotate = (addr & 3) * 8;
                if (rotate) {
                    val = (val >> rotate) | (val << (32 - rotate));
                }
                cpu->r[rd] = val;
            }
            if (rd == 15) {
                arm7tdmi_fill_pipeline_arm(cpu);
                return 3;
            }
        } else {
            uint32_t val = cpu->r[rd];
            if (rd == 15) val += 4;
            if (byte) {
                bus_write8(&cpu->gba->bus, addr, (uint8_t)val);
            } else {
                bus_write32(&cpu->gba->bus, addr & ~3U, val);
            }
        }

        if (!pre) {
            addr = up ? (addr + offset) : (addr - offset);
            cpu->r[rn] = addr;
        } else if (writeback) {
            cpu->r[rn] = addr;
        }

        return load ? 2 : 1;
    }

    // 11. Data Processing
    if ((instr & 0x0C000000) == 0x00000000) {
        bool imm = (instr & (1 << 25)) != 0;
        uint8_t opcode = (instr >> 21) & 0xF;
        bool s_bit = (instr & (1 << 20)) != 0;
        uint8_t rn = (instr >> 16) & 0xF;
        uint8_t rd = (instr >> 12) & 0xF;

        uint32_t op2;
        bool shifter_carry = (cpu->cpsr & FLAG_C) != 0;

        if (imm) {
            op2 = instr & 0xFF;
            uint32_t rot = ((instr >> 8) & 0xF) * 2;
            if (rot) {
                op2 = (op2 >> rot) | (op2 << (32 - rot));
                shifter_carry = (op2 >> 31) & 1;
            }
        } else {
            uint8_t shift_type = (instr >> 5) & 0x3;
            bool reg_shift = (instr & (1 << 4)) != 0;
            uint32_t shift_amount;
            if (reg_shift) {
                uint8_t rs = (instr >> 8) & 0xF;
                shift_amount = cpu->r[rs] & 0xFF;
                // If PC is Rn, it is already advanced
            } else {
                shift_amount = (instr >> 7) & 0x1F;
            }
            op2 = arm_shift(cpu, shift_type, cpu->r[instr & 0xF], shift_amount, &shifter_carry, !reg_shift);
        }

        uint32_t op1 = cpu->r[rn];
        uint32_t res = 0;
        bool write_rd = true;

        switch (opcode) {
            case 0x0: // AND
                res = op1 & op2;
                if (s_bit) alu_set_logic_flags(cpu, res, shifter_carry);
                break;
            case 0x1: // EOR
                res = op1 ^ op2;
                if (s_bit) alu_set_logic_flags(cpu, res, shifter_carry);
                break;
            case 0x2: // SUB
                res = alu_sub(cpu, op1, op2, 1, s_bit);
                break;
            case 0x3: // RSB
                res = alu_sub(cpu, op2, op1, 1, s_bit);
                break;
            case 0x4: // ADD
                res = alu_add(cpu, op1, op2, 0, s_bit);
                break;
            case 0x5: // ADC
                res = alu_add(cpu, op1, op2, (cpu->cpsr & FLAG_C) ? 1 : 0, s_bit);
                break;
            case 0x6: // SBC
                res = alu_sub(cpu, op1, op2, (cpu->cpsr & FLAG_C) ? 1 : 0, s_bit);
                break;
            case 0x7: // RSC
                res = alu_sub(cpu, op2, op1, (cpu->cpsr & FLAG_C) ? 1 : 0, s_bit);
                break;
            case 0x8: // TST
                res = op1 & op2;
                alu_set_logic_flags(cpu, res, shifter_carry);
                write_rd = false;
                break;
            case 0x9: // TEQ
                res = op1 ^ op2;
                alu_set_logic_flags(cpu, res, shifter_carry);
                write_rd = false;
                break;
            case 0xA: // CMP
                alu_sub(cpu, op1, op2, 1, true);
                write_rd = false;
                break;
            case 0xB: // CMN
                alu_add(cpu, op1, op2, 0, true);
                write_rd = false;
                break;
            case 0xC: // ORR
                res = op1 | op2;
                if (s_bit) alu_set_logic_flags(cpu, res, shifter_carry);
                break;
            case 0xD: // MOV
                res = op2;
                if (s_bit) alu_set_logic_flags(cpu, res, shifter_carry);
                break;
            case 0xE: // BIC
                res = op1 & ~op2;
                if (s_bit) alu_set_logic_flags(cpu, res, shifter_carry);
                break;
            case 0xF: // MVN
                res = ~op2;
                if (s_bit) alu_set_logic_flags(cpu, res, shifter_carry);
                break;
        }

        if (write_rd) {
            if (rd == 15) {
                if (s_bit) {
                    uint32_t* pspsr = arm7tdmi_get_spsr(cpu);
                    if ((cpu->cpsr & MODE_MASK) != (*pspsr & MODE_MASK)) {
                        arm7tdmi_switch_mode(cpu, (CpuMode)(*pspsr & MODE_MASK));
                    }
                    cpu->cpsr = *pspsr;
                }
                cpu->r[15] = res;
                if (cpu->cpsr & FLAG_T) {
                    arm7tdmi_fill_pipeline_thumb(cpu);
                } else {
                    arm7tdmi_fill_pipeline_arm(cpu);
                }
                return 3;
            } else {
                cpu->r[rd] = res;
            }
        }
        return 1;
    }

    return 1;
}
