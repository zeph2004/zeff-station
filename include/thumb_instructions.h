#ifndef ZEFF_THUMB_INSTRUCTIONS_H
#define ZEFF_THUMB_INSTRUCTIONS_H

#include <stdint.h>
#include "arm7tdmi.h"

// Executes a single 16-bit THUMB instruction and returns cycles taken
int thumb_execute_instruction(ARM7TDMI* cpu, uint16_t instr);

#endif // ZEFF_THUMB_INSTRUCTIONS_H
