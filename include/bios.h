#ifndef ZEFF_BIOS_H
#define ZEFF_BIOS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct GBA GBA;
typedef struct ARM7TDMI ARM7TDMI;

void bios_handle_swi(ARM7TDMI* cpu, uint8_t comment);
void bios_render_intro_frame(GBA* gba, uint32_t frame_index);
void bios_setup_post_boot(GBA* gba);

#endif // ZEFF_BIOS_H
