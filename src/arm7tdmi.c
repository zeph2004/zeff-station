#include "arm7tdmi.h"
#include "arm_instructions.h"
#include "thumb_instructions.h"
#include "gba.h"
#include <string.h>
#include <stdio.h>

void arm7tdmi_init(ARM7TDMI* cpu, GBA* gba) {
    memset(cpu, 0, sizeof(ARM7TDMI));
    cpu->gba = gba;
    cpu->cpsr = MODE_SYS;
}

void arm7tdmi_reset(ARM7TDMI* cpu, uint32_t entry_point, bool thumb) {
    cpu->cpsr = MODE_SYS;
    if (thumb) {
        cpu->cpsr |= FLAG_T;
    } else {
        cpu->cpsr &= ~FLAG_T;
    }

    cpu->sp_usr = 0x03007F00;
    cpu->sp_irq = 0x03007FA0;
    cpu->sp_svc = 0x03007FE0;
    cpu->r[13] = cpu->sp_usr;

    cpu->r[15] = entry_point;
    if (thumb) {
        arm7tdmi_fill_pipeline_thumb(cpu);
    } else {
        arm7tdmi_fill_pipeline_arm(cpu);
    }
}

void arm7tdmi_switch_mode(ARM7TDMI* cpu, CpuMode new_mode) {
    CpuMode old_mode = (CpuMode)(cpu->cpsr & MODE_MASK);
    if (old_mode == new_mode) {
        return;
    }

    // Save registers of old mode
    if (old_mode == MODE_FIQ) {
        cpu->r8_fiq = cpu->r[8];
        cpu->r9_fiq = cpu->r[9];
        cpu->r10_fiq = cpu->r[10];
        cpu->r11_fiq = cpu->r[11];
        cpu->r12_fiq = cpu->r[12];
        cpu->sp_fiq = cpu->r[13];
        cpu->lr_fiq = cpu->r[14];
    } else {
        if (old_mode == MODE_USR || old_mode == MODE_SYS) {
            cpu->r8_usr = cpu->r[8];
            cpu->r9_usr = cpu->r[9];
            cpu->r10_usr = cpu->r[10];
            cpu->r11_usr = cpu->r[11];
            cpu->r12_usr = cpu->r[12];
            cpu->sp_usr = cpu->r[13];
            cpu->lr_usr = cpu->r[14];
        } else if (old_mode == MODE_IRQ) {
            cpu->r8_usr = cpu->r[8];
            cpu->r9_usr = cpu->r[9];
            cpu->r10_usr = cpu->r[10];
            cpu->r11_usr = cpu->r[11];
            cpu->r12_usr = cpu->r[12];
            cpu->sp_irq = cpu->r[13];
            cpu->lr_irq = cpu->r[14];
        } else if (old_mode == MODE_SVC) {
            cpu->r8_usr = cpu->r[8];
            cpu->r9_usr = cpu->r[9];
            cpu->r10_usr = cpu->r[10];
            cpu->r11_usr = cpu->r[11];
            cpu->r12_usr = cpu->r[12];
            cpu->sp_svc = cpu->r[13];
            cpu->lr_svc = cpu->r[14];
        } else if (old_mode == MODE_ABT) {
            cpu->r8_usr = cpu->r[8];
            cpu->r9_usr = cpu->r[9];
            cpu->r10_usr = cpu->r[10];
            cpu->r11_usr = cpu->r[11];
            cpu->r12_usr = cpu->r[12];
            cpu->sp_abt = cpu->r[13];
            cpu->lr_abt = cpu->r[14];
        } else if (old_mode == MODE_UND) {
            cpu->r8_usr = cpu->r[8];
            cpu->r9_usr = cpu->r[9];
            cpu->r10_usr = cpu->r[10];
            cpu->r11_usr = cpu->r[11];
            cpu->r12_usr = cpu->r[12];
            cpu->sp_und = cpu->r[13];
            cpu->lr_und = cpu->r[14];
        }
    }

    // Restore registers of new mode
    if (new_mode == MODE_FIQ) {
        cpu->r[8]  = cpu->r8_fiq;
        cpu->r[9]  = cpu->r9_fiq;
        cpu->r[10] = cpu->r10_fiq;
        cpu->r[11] = cpu->r11_fiq;
        cpu->r[12] = cpu->r12_fiq;
        cpu->r[13] = cpu->sp_fiq;
        cpu->r[14] = cpu->lr_fiq;
    } else {
        cpu->r[8]  = cpu->r8_usr;
        cpu->r[9]  = cpu->r9_usr;
        cpu->r[10] = cpu->r10_usr;
        cpu->r[11] = cpu->r11_usr;
        cpu->r[12] = cpu->r12_usr;
        if (new_mode == MODE_USR || new_mode == MODE_SYS) {
            cpu->r[13] = cpu->sp_usr;
            cpu->r[14] = cpu->lr_usr;
        } else if (new_mode == MODE_IRQ) {
            cpu->r[13] = cpu->sp_irq;
            cpu->r[14] = cpu->lr_irq;
        } else if (new_mode == MODE_SVC) {
            cpu->r[13] = cpu->sp_svc;
            cpu->r[14] = cpu->lr_svc;
        } else if (new_mode == MODE_ABT) {
            cpu->r[13] = cpu->sp_abt;
            cpu->r[14] = cpu->lr_abt;
        } else if (new_mode == MODE_UND) {
            cpu->r[13] = cpu->sp_und;
            cpu->r[14] = cpu->lr_und;
        }
    }

    cpu->cpsr = (cpu->cpsr & ~MODE_MASK) | (uint32_t)new_mode;
}

uint32_t* arm7tdmi_get_spsr(ARM7TDMI* cpu) {
    switch (cpu->cpsr & MODE_MASK) {
        case MODE_FIQ: return &cpu->spsr_fiq;
        case MODE_IRQ: return &cpu->spsr_irq;
        case MODE_SVC: return &cpu->spsr_svc;
        case MODE_ABT: return &cpu->spsr_abt;
        case MODE_UND: return &cpu->spsr_und;
        default: return &cpu->cpsr;
    }
}

bool arm7tdmi_check_condition(const ARM7TDMI* cpu, uint8_t cond) {
    uint32_t cpsr = cpu->cpsr;
    bool n = (cpsr & FLAG_N) != 0;
    bool z = (cpsr & FLAG_Z) != 0;
    bool c = (cpsr & FLAG_C) != 0;
    bool v = (cpsr & FLAG_V) != 0;

    switch (cond) {
        case 0x0: return z;                       // EQ
        case 0x1: return !z;                      // NE
        case 0x2: return c;                       // CS / HS
        case 0x3: return !c;                      // CC / LO
        case 0x4: return n;                       // MI
        case 0x5: return !n;                      // PL
        case 0x6: return v;                       // VS
        case 0x7: return !v;                      // VC
        case 0x8: return c && !z;                 // HI
        case 0x9: return !c || z;                 // LS
        case 0xA: return n == v;                  // GE
        case 0xB: return n != v;                  // LT
        case 0xC: return !z && (n == v);          // GT
        case 0xD: return z || (n != v);           // LE
        case 0xE: return true;                    // AL
        case 0xF: return false;                   // NV (reserved)
        default: return false;
    }
}

void arm7tdmi_fill_pipeline_arm(ARM7TDMI* cpu) {
    uint32_t target = cpu->r[15] & ~3U;
    cpu->pipeline[0] = bus_read32(&cpu->gba->bus, target);
    cpu->pipeline[1] = bus_read32(&cpu->gba->bus, target + 4);
    cpu->r[15] = target + 8;
    cpu->branched = true;
}

void arm7tdmi_fill_pipeline_thumb(ARM7TDMI* cpu) {
    uint32_t target = cpu->r[15] & ~1U;
    cpu->pipeline[0] = bus_read16(&cpu->gba->bus, target);
    cpu->pipeline[1] = bus_read16(&cpu->gba->bus, target + 2);
    cpu->r[15] = target + 4;
    cpu->branched = true;
}

void arm7tdmi_trigger_irq(ARM7TDMI* cpu) {
    if (cpu->cpsr & FLAG_I) {
        return; // IRQ masked
    }

    // Set BIOS interrupt check flags at 0x03007FF8
    uint16_t irq_flags = cpu->gba->interrupt.ie & cpu->gba->interrupt.if_;
    *(uint16_t*)&cpu->gba->bus.iwram[0x7FF8] |= irq_flags;

    uint32_t old_cpsr = cpu->cpsr;
    uint32_t return_pc = (old_cpsr & FLAG_T) ? cpu->r[15] : (cpu->r[15] - 4);

    arm7tdmi_switch_mode(cpu, MODE_IRQ);
    cpu->spsr_irq = old_cpsr;
    cpu->r[14] = return_pc;
    cpu->lr_irq = return_pc;

    cpu->cpsr |= FLAG_I;   // Disable IRQ
    cpu->cpsr &= ~FLAG_T;  // Enter ARM state

    cpu->r[15] = 0x00000018;
    arm7tdmi_fill_pipeline_arm(cpu);
}

void arm7tdmi_trigger_swi(ARM7TDMI* cpu, uint8_t comment) {
    // If BIOS is not loaded or we are using HLE BIOS:
    if (!cpu->gba->bus.bios_loaded) {
        bios_handle_swi(cpu, comment);
        return;
    }

    uint32_t old_cpsr = cpu->cpsr;
    uint32_t return_pc = (old_cpsr & FLAG_T) ? (cpu->r[15] - 2) : (cpu->r[15] - 4);

    arm7tdmi_switch_mode(cpu, MODE_SVC);
    cpu->spsr_svc = old_cpsr;
    cpu->r[14] = return_pc;
    cpu->lr_svc = return_pc;

    cpu->cpsr |= FLAG_I;
    cpu->cpsr &= ~FLAG_T;

    cpu->r[15] = 0x00000008;
    arm7tdmi_fill_pipeline_arm(cpu);
}

int arm7tdmi_step(ARM7TDMI* cpu) {
    // Check interrupts
    if (!(cpu->cpsr & FLAG_I)) {
        if ((cpu->gba->interrupt.ime & 1) && 
            (cpu->gba->interrupt.ie & cpu->gba->interrupt.if_)) {
            arm7tdmi_trigger_irq(cpu);
            cpu->gba->halted = false;
            return 3;
        }
    }

    if (cpu->gba->halted) {
        return 1;
    }

    cpu->cycles_spent = 0;

    if (cpu->cpsr & FLAG_T) {
        uint16_t instr = (uint16_t)cpu->pipeline[0];
        cpu->branched = false;
        int c = thumb_execute_instruction(cpu, instr);
        if (!cpu->branched) {
            cpu->pipeline[0] = cpu->pipeline[1];
            cpu->pipeline[1] = bus_read16(&cpu->gba->bus, cpu->r[15]);
            cpu->r[15] += 2;
        }
        return c > 0 ? c : 1;
    } else {
        uint32_t instr = cpu->pipeline[0];
        cpu->branched = false;
        int c = arm_execute_instruction(cpu, instr);
        if (!cpu->branched) {
            cpu->pipeline[0] = cpu->pipeline[1];
            cpu->pipeline[1] = bus_read32(&cpu->gba->bus, cpu->r[15]);
            cpu->r[15] += 4;
        }
        return c > 0 ? c : 1;
    }
}
