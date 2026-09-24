#ifndef ZEFF_ARM_INSTRUCTIONS_H
#define ZEFF_ARM_INSTRUCTIONS_H

#include <stdint.h>
#include "arm7tdmi.h"

// Executes a single 32-bit ARM instruction and returns cycles taken
int arm_execute_instruction(ARM7TDMI* cpu, uint32_t instr);

#endif // ZEFF_ARM_INSTRUCTIONS_H
