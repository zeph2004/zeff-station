#ifndef ZEFF_ARM7TDMI_H
#define ZEFF_ARM7TDMI_H

#include <stdint.h>
#include <stdbool.h>

// Forward declaration
typedef struct GBA GBA;

typedef enum {
    MODE_USR = 0x10,
    MODE_FIQ = 0x11,
    MODE_IRQ = 0x12,
    MODE_SVC = 0x13,
    MODE_ABT = 0x17,
    MODE_UND = 0x1B,
    MODE_SYS = 0x1F
} CpuMode;

// CPSR flag bit positions
#define FLAG_N (1U << 31) // Negative
#define FLAG_Z (1U << 30) // Zero
#define FLAG_C (1U << 29) // Carry / not borrow
#define FLAG_V (1U << 28) // Overflow
#define FLAG_I (1U << 7)  // IRQ disable
#define FLAG_F (1U << 6)  // FIQ disable
#define FLAG_T (1U << 5)  // THUMB state bit
#define MODE_MASK 0x1F

typedef struct ARM7TDMI {
    // Current visible registers (R0-R15)
    uint32_t r[16];
    uint32_t cpsr;

    // Banked registers
    uint32_t r8_fiq, r9_fiq, r10_fiq, r11_fiq, r12_fiq;
    uint32_t r8_usr, r9_usr, r10_usr, r11_usr, r12_usr;

    // SP (R13) banked for each mode
    uint32_t sp_usr;
    uint32_t sp_fiq;
    uint32_t sp_irq;
    uint32_t sp_svc;
    uint32_t sp_abt;
    uint32_t sp_und;

    // LR (R14) banked for each mode
    uint32_t lr_usr;
    uint32_t lr_fiq;
    uint32_t lr_irq;
    uint32_t lr_svc;
    uint32_t lr_abt;
    uint32_t lr_und;

    // Saved Program Status Registers (SPSR)
    uint32_t spsr_fiq;
    uint32_t spsr_irq;
    uint32_t spsr_svc;
    uint32_t spsr_abt;
    uint32_t spsr_und;

    // Pipeline prefetched instructions
    uint32_t pipeline[2];

    // Reference to parent system
    GBA* gba;

    // Cycle accounting for current instruction
    int cycles_spent;
    bool branched;
} ARM7TDMI;

void arm7tdmi_init(ARM7TDMI* cpu, GBA* gba);
void arm7tdmi_reset(ARM7TDMI* cpu, uint32_t entry_point, bool thumb);
void arm7tdmi_switch_mode(ARM7TDMI* cpu, CpuMode new_mode);
uint32_t* arm7tdmi_get_spsr(ARM7TDMI* cpu);
void arm7tdmi_trigger_irq(ARM7TDMI* cpu);
void arm7tdmi_trigger_swi(ARM7TDMI* cpu, uint8_t comment);

// Execution
int arm7tdmi_step(ARM7TDMI* cpu);
bool arm7tdmi_check_condition(const ARM7TDMI* cpu, uint8_t cond);

// Pipeline management
void arm7tdmi_fill_pipeline_arm(ARM7TDMI* cpu);
void arm7tdmi_fill_pipeline_thumb(ARM7TDMI* cpu);

#endif // ZEFF_ARM7TDMI_H
