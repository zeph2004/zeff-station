#ifndef ZEFF_GBA_H
#define ZEFF_GBA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define GBA_SCREEN_WIDTH   240
#define GBA_SCREEN_HEIGHT  160
#define GBA_SCREEN_PIXELS  (GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT)

#define GBA_CLOCK_FREQ     16777216 // 16.777216 MHz
#define CYCLES_PER_SCANLINE 1232
#define SCANLINES_PER_FRAME 228
#define VISIBLE_SCANLINES   160
#define CYCLES_PER_FRAME    (CYCLES_PER_SCANLINE * SCANLINES_PER_FRAME) // 280,896

#define EWRAM_SIZE   (256 * 1024)   // 256 KB
#define IWRAM_SIZE   (32 * 1024)    // 32 KB
#define PALETTE_SIZE (1 * 1024)     // 1 KB
#define VRAM_SIZE    (96 * 1024)    // 96 KB
#define OAM_SIZE     (1 * 1024)     // 1 KB
#define BIOS_SIZE    (16 * 1024)    // 16 KB
#define ROM_MAX_SIZE (32 * 1024 * 1024) // 32 MB
#define SRAM_MAX_SIZE (64 * 1024)   // 64 KB

// Forward declarations
typedef struct GBA GBA;
typedef struct ARM7TDMI ARM7TDMI;
typedef struct PPU PPU;
typedef struct Bus Bus;
typedef struct DMAController DMAController;
typedef struct TimerController TimerController;
typedef struct InterruptController InterruptController;
typedef struct APU APU;
typedef struct Keypad Keypad;

#include "arm7tdmi.h"
#include "bus.h"
#include "ppu.h"
#include "dma.h"
#include "timer.h"
#include "interrupt.h"
#include "apu.h"
#include "keypad.h"
#include "bios.h"

struct GBA {
    ARM7TDMI cpu;
    Bus bus;
    PPU ppu;
    DMAController dma;
    TimerController timers;
    InterruptController interrupt;
    APU apu;
    Keypad keypad;

    // Framebuffer in 32-bit RGBA (0xRRGGBB or 0xAABBGGRR)
    uint32_t framebuffer[GBA_SCREEN_PIXELS];

    // Master cycle counter
    uint64_t total_cycles;
    uint32_t frame_cycles;
    uint32_t scanline_cycles;
    uint32_t current_scanline;

    bool halted;
    bool stopped;
    bool frame_ready;

    // Boot & Branding
    bool skip_intro;
    bool intro_completed;
    uint32_t intro_frame_counter;

    char rom_title[16];
    char save_filepath[1024];
};

// System lifecycle
GBA* gba_create(void);
void gba_destroy(GBA* gba);
void gba_reset(GBA* gba, bool skip_intro);
bool gba_load_rom(GBA* gba, const char* rom_path);
bool gba_load_bios(GBA* gba, const char* bios_path);

// Execution
void gba_step(GBA* gba);
void gba_run_frame(GBA* gba);

#endif // ZEFF_GBA_H
