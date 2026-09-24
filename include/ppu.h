#ifndef ZEFF_PPU_H
#define ZEFF_PPU_H

#include <stdint.h>
#include <stdbool.h>

typedef struct GBA GBA;

typedef struct PPU {
    // Registers
    uint16_t dispcnt;
    uint16_t dispstat;
    uint16_t vcount;

    uint16_t bgcnt[4];
    uint16_t bghofs[4];
    uint16_t bgvofs[4];

    // Affine parameters for BG2 and BG3
    int16_t bgpa[2]; // BG2PA, BG3PA
    int16_t bgpb[2]; // BG2PB, BG3PB
    int16_t bgpc[2]; // BG2PC, BG3PC
    int16_t bgpd[2]; // BG2PD, BG3PD
    int32_t bgx[2];  // BG2X, BG3X (28-bit signed fixed point 19.8)
    int32_t bgy[2];  // BG2Y, BG3Y (28-bit signed fixed point 19.8)

    // Internal reference point latches
    int32_t bgx_internal[2];
    int32_t bgy_internal[2];

    // Windows
    uint16_t win0h;
    uint16_t win1h;
    uint16_t win0v;
    uint16_t win1v;
    uint16_t winin;
    uint16_t winout;

    // Special effects
    uint16_t bldcnt;
    uint16_t bldalpha;
    uint16_t bldy;
    uint16_t mosaic;

    // Line pixel buffers for compositing
    // Bits 0-14: RGB555 color
    // Bits 16-19: Layer ID (0=BG0, 1=BG1, 2=BG2, 3=BG3, 4=OBJ, 5=BD)
    // Bits 20-21: Priority (0-3)
    // Bit 22: Alpha semi-transparent flag
    uint32_t bg_scanline[4][240];
    bool bg_scanline_valid[4][240];
    uint32_t obj_scanline[240];
    bool obj_scanline_valid[240];
    bool obj_window_mask[240];

    GBA* gba;
} PPU;

void ppu_init(PPU* ppu, GBA* gba);
void ppu_reset(PPU* ppu);
void ppu_step(PPU* ppu, int cycles);
void ppu_render_scanline(PPU* ppu, int line);

void ppu_write16(PPU* ppu, uint32_t addr, uint16_t val);
uint16_t ppu_read16(PPU* ppu, uint32_t addr);

#endif // ZEFF_PPU_H
