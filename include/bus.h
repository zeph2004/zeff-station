#ifndef ZEFF_BUS_H
#define ZEFF_BUS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct GBA GBA;

// I/O Register Offsets (0x04000000 + ...)
#define REG_DISPCNT      0x0000
#define REG_DISPSTAT     0x0004
#define REG_VCOUNT       0x0006
#define REG_BG0CNT       0x0008
#define REG_BG1CNT       0x000A
#define REG_BG2CNT       0x000C
#define REG_BG3CNT       0x000E
#define REG_BG0HOFS      0x0010
#define REG_BG0VOFS      0x0012
#define REG_BG1HOFS      0x0014
#define REG_BG1VOFS      0x0016
#define REG_BG2HOFS      0x0018
#define REG_BG2VOFS      0x001A
#define REG_BG3HOFS      0x001C
#define REG_BG3VOFS      0x001E
#define REG_BG2PA        0x0020
#define REG_BG2PB        0x0022
#define REG_BG2PC        0x0024
#define REG_BG2PD        0x0026
#define REG_BG2X_L       0x0028
#define REG_BG2X_H       0x002A
#define REG_BG2Y_L       0x002C
#define REG_BG2Y_H       0x002E
#define REG_BG3PA        0x0030
#define REG_BG3PB        0x0032
#define REG_BG3PC        0x0034
#define REG_BG3PD        0x0036
#define REG_BG3X_L       0x0038
#define REG_BG3X_H       0x003A
#define REG_BG3Y_L       0x003C
#define REG_BG3Y_H       0x003E
#define REG_WIN0H        0x0040
#define REG_WIN1H        0x0042
#define REG_WIN0V        0x0044
#define REG_WIN1V        0x0046
#define REG_WININ        0x0048
#define REG_WINOUT       0x004A
#define REG_MOSAIC       0x004C
#define REG_BLDCNT       0x0050
#define REG_BLDALPHA     0x0052
#define REG_BLDY         0x0054

// Sound Registers
#define REG_SOUND1CNT_L  0x0060
#define REG_SOUND1CNT_H  0x0062
#define REG_SOUND1CNT_X  0x0064
#define REG_SOUND2CNT_L  0x0068
#define REG_SOUND2CNT_H  0x006C
#define REG_SOUND3CNT_L  0x0070
#define REG_SOUND3CNT_H  0x0072
#define REG_SOUND3CNT_X  0x0074
#define REG_SOUND4CNT_L  0x0078
#define REG_SOUND4CNT_H  0x007C
#define REG_SOUNDCNT_L   0x0080
#define REG_SOUNDCNT_H   0x0082
#define REG_SOUNDCNT_X   0x0084
#define REG_SOUNDBIAS    0x0088
#define REG_WAVE_RAM     0x0090
#define REG_FIFO_A       0x00A0
#define REG_FIFO_B       0x00A4

// DMA Registers
#define REG_DMA0SAD_L    0x00B0
#define REG_DMA0SAD_H    0x00B2
#define REG_DMA0DAD_L    0x00B4
#define REG_DMA0DAD_H    0x00B6
#define REG_DMA0CNT_L    0x00B8
#define REG_DMA0CNT_H    0x00BA

#define REG_DMA1SAD_L    0x00BC
#define REG_DMA1SAD_H    0x00BE
#define REG_DMA1DAD_L    0x00C0
#define REG_DMA1DAD_H    0x00C2
#define REG_DMA1CNT_L    0x00C4
#define REG_DMA1CNT_H    0x00C6

#define REG_DMA2SAD_L    0x00C8
#define REG_DMA2SAD_H    0x00CA
#define REG_DMA2DAD_L    0x00CC
#define REG_DMA2DAD_H    0x00CE
#define REG_DMA2CNT_L    0x00D0
#define REG_DMA2CNT_H    0x00D2

#define REG_DMA3SAD_L    0x00D4
#define REG_DMA3SAD_H    0x00D6
#define REG_DMA3DAD_L    0x00D8
#define REG_DMA3DAD_H    0x00DA
#define REG_DMA3CNT_L    0x00DC
#define REG_DMA3CNT_H    0x00DE

// Timer Registers
#define REG_TM0CNT_L     0x0100
#define REG_TM0CNT_H     0x0102
#define REG_TM1CNT_L     0x0104
#define REG_TM1CNT_H     0x0106
#define REG_TM2CNT_L     0x0108
#define REG_TM2CNT_H     0x010A
#define REG_TM3CNT_L     0x010C
#define REG_TM3CNT_H     0x010E

// Keypad Registers
#define REG_KEYINPUT     0x0130
#define REG_KEYCNT       0x0132

// Interrupt & System Control
#define REG_IE           0x0200
#define REG_IF           0x0202
#define REG_WAITCNT      0x0204
#define REG_IME          0x0208
#define REG_POSTFLG      0x0300
#define REG_HALTCNT      0x0301

typedef struct Bus {
    uint8_t bios[16 * 1024];
    uint8_t ewram[256 * 1024];
    uint8_t iwram[32 * 1024];
    uint8_t io[1024];
    uint8_t pram[1024];
    uint8_t vram[96 * 1024];
    uint8_t oam[1024];

    uint8_t* rom;
    size_t rom_size;

    uint8_t sram[64 * 1024];
    bool sram_dirty;
    bool has_sram;

    uint8_t eeprom[8192];
    uint16_t eeprom_read_addr;
    bool has_eeprom;

    bool bios_loaded;
    GBA* gba;
} Bus;

void bus_init(Bus* bus, GBA* gba);
void bus_destroy(Bus* bus);
void bus_reset(Bus* bus);

// Memory Access
uint8_t  bus_read8(Bus* bus, uint32_t addr);
uint16_t bus_read16(Bus* bus, uint32_t addr);
uint32_t bus_read32(Bus* bus, uint32_t addr);

void bus_write8(Bus* bus, uint32_t addr, uint8_t val);
void bus_write16(Bus* bus, uint32_t addr, uint16_t val);
void bus_write32(Bus* bus, uint32_t addr, uint32_t val);

// Direct pointer access for fast HLE and rendering
uint8_t* bus_get_ptr(Bus* bus, uint32_t addr);

// SRAM save persistence
void bus_save_sram(Bus* bus, const char* filepath);
void bus_load_sram(Bus* bus, const char* filepath);

#endif // ZEFF_BUS_H
