#include "bios.h"
#include "gba.h"
#include "bus.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// 5x7 Retro Font for Zeff Station intro
static const uint8_t font_5x7[][5] = {
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00},
    ['A'] = {0x7C, 0x12, 0x11, 0x12, 0x7C},
    ['B'] = {0x7F, 0x49, 0x49, 0x49, 0x36},
    ['C'] = {0x3E, 0x41, 0x41, 0x41, 0x22},
    ['D'] = {0x7F, 0x41, 0x41, 0x22, 0x1C},
    ['E'] = {0x7F, 0x49, 0x49, 0x49, 0x41},
    ['F'] = {0x7F, 0x09, 0x09, 0x09, 0x01},
    ['G'] = {0x3E, 0x41, 0x49, 0x49, 0x7A},
    ['H'] = {0x7F, 0x08, 0x08, 0x08, 0x7F},
    ['I'] = {0x00, 0x41, 0x7F, 0x41, 0x00},
    ['J'] = {0x20, 0x40, 0x41, 0x3F, 0x01},
    ['K'] = {0x7F, 0x08, 0x14, 0x22, 0x41},
    ['L'] = {0x7F, 0x40, 0x40, 0x40, 0x40},
    ['M'] = {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    ['N'] = {0x7F, 0x04, 0x08, 0x10, 0x7F},
    ['O'] = {0x3E, 0x41, 0x41, 0x41, 0x3E},
    ['P'] = {0x7F, 0x09, 0x09, 0x09, 0x06},
    ['Q'] = {0x3E, 0x41, 0x51, 0x21, 0x5E},
    ['R'] = {0x7F, 0x09, 0x19, 0x29, 0x46},
    ['S'] = {0x46, 0x49, 0x49, 0x49, 0x31},
    ['T'] = {0x01, 0x01, 0x7F, 0x01, 0x01},
    ['U'] = {0x3F, 0x40, 0x40, 0x40, 0x3F},
    ['V'] = {0x1F, 0x20, 0x40, 0x20, 0x1F},
    ['W'] = {0x7F, 0x20, 0x18, 0x20, 0x7F},
    ['X'] = {0x63, 0x14, 0x08, 0x14, 0x63},
    ['Y'] = {0x07, 0x08, 0x70, 0x08, 0x07},
    ['Z'] = {0x61, 0x51, 0x49, 0x45, 0x43},
    ['0'] = {0x3E, 0x51, 0x49, 0x45, 0x3E},
    ['1'] = {0x00, 0x42, 0x7F, 0x40, 0x00},
    ['2'] = {0x42, 0x61, 0x51, 0x49, 0x46},
    ['3'] = {0x21, 0x41, 0x45, 0x4B, 0x31},
    ['4'] = {0x18, 0x14, 0x12, 0x7F, 0x10},
    ['5'] = {0x27, 0x45, 0x45, 0x45, 0x39},
    ['6'] = {0x3C, 0x4A, 0x49, 0x49, 0x30},
    ['7'] = {0x01, 0x71, 0x09, 0x05, 0x03},
    ['8'] = {0x36, 0x49, 0x49, 0x49, 0x36},
    ['9'] = {0x06, 0x49, 0x49, 0x29, 0x1E},
    ['.'] = {0x00, 0x60, 0x60, 0x00, 0x00},
    [':'] = {0x00, 0x36, 0x36, 0x00, 0x00},
    ['-'] = {0x08, 0x08, 0x08, 0x08, 0x08},
};

static void draw_char(uint32_t* fb, int x, int y, char c, int scale, uint32_t color) {
    if ((unsigned char)c >= 128) return;
    const uint8_t* glyph = font_5x7[(int)c];
    for (int col = 0; col < 5; col++) {
        uint8_t line = glyph[col];
        for (int row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        int px = x + col * scale + sx;
                        int py = y + row * scale + sy;
                        if (px >= 0 && px < GBA_SCREEN_WIDTH && py >= 0 && py < GBA_SCREEN_HEIGHT) {
                            fb[py * GBA_SCREEN_WIDTH + px] = color;
                        }
                    }
                }
            }
        }
    }
}

static void draw_string(uint32_t* fb, int x, int y, const char* str, int scale, uint32_t color) {
    while (*str) {
        draw_char(fb, x, y, *str, scale, color);
        x += (5 + 1) * scale;
        str++;
    }
}

void bios_render_intro_frame(GBA* gba, uint32_t frame_index) {
    uint32_t* fb = gba->framebuffer;

    // Deep space background gradient
    for (int y = 0; y < GBA_SCREEN_HEIGHT; y++) {
        uint8_t blue = (uint8_t)(10 + (y * 25) / GBA_SCREEN_HEIGHT);
        uint32_t bg_col = 0xFF000000 | ((uint32_t)blue << 16) | (5 << 8) | 5;
        for (int x = 0; x < GBA_SCREEN_WIDTH; x++) {
            fb[y * GBA_SCREEN_WIDTH + x] = bg_col;
        }
    }

    // Starfield animation
    for (int i = 0; i < 40; i++) {
        int sx = (i * 37 + (int)(frame_index * (1 + (i % 3)))) % GBA_SCREEN_WIDTH;
        int sy = (i * 59 + i * 11) % GBA_SCREEN_HEIGHT;
        uint8_t star_bright = (uint8_t)(160 + (i * 19) % 95);
        uint32_t star_col = 0xFF000000 | (star_bright << 16) | (star_bright << 8) | star_bright;
        fb[sy * GBA_SCREEN_WIDTH + sx] = star_col;
    }

    // Title: "ZEFF STATION" swooping into center
    int target_y = 60;
    int cur_y = target_y;
    if (frame_index < 30) {
        cur_y = target_y - (30 - (int)frame_index) * 2;
    }

    // Animated glow pulse
    float pulse = (float)sin(frame_index * 0.15f);
    uint8_t r = (uint8_t)(220 + pulse * 35.0f);
    uint8_t g = (uint8_t)(180 + pulse * 40.0f);
    uint8_t b = (uint8_t)(40 + pulse * 20.0f);
    uint32_t gold_glow = 0xFF000000 | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;

    // Draw shadow / glow
    draw_string(fb, 26, cur_y + 2, "ZEFF STATION", 3, 0xFF662200);
    // Draw main title
    draw_string(fb, 24, cur_y, "ZEFF STATION", 3, gold_glow);

    // Subtitle: "ADVANCE EMULATOR"
    if (frame_index >= 20) {
        uint32_t cyan = 0xFFEEEE00 | (uint32_t)(180 + pulse * 60.0f);
        draw_string(fb, 66, cur_y + 32, "ADVANCE SYSTEM", 1, cyan);
    }

    // ROM title below
    if (frame_index >= 35 && gba->rom_title[0] != '\0') {
        char buf[64];
        snprintf(buf, sizeof(buf), "GAME: %s", gba->rom_title);
        draw_string(fb, 45, 125, buf, 1, 0xFF88FF88);
    }

    // Audio chime during frames 10-60
    if (frame_index >= 10 && frame_index <= 50) {
        gba->apu.soundcnt_x = 0x80;
        gba->apu.soundcnt_l = 0x77;
        gba->apu.ch1.enabled = true;
        gba->apu.ch1.duty = 2; // 50%
        gba->apu.ch1.env_volume = 10;
        gba->apu.ch1.freq = (frame_index < 30) ? 1650 : 1820; // 2-tone melodic chime
        gba->apu.ch1.timer = (2048 - gba->apu.ch1.freq) * 4;
    } else if (frame_index == 51) {
        gba->apu.ch1.enabled = false;
    }
}

void bios_setup_post_boot(GBA* gba) {
    ARM7TDMI* cpu = &gba->cpu;
    cpu->sp_usr = 0x03007F00;
    cpu->sp_irq = 0x03007FA0;
    cpu->sp_svc = 0x03007FE0;

    arm7tdmi_switch_mode(cpu, MODE_SYS);
    cpu->r[13] = cpu->sp_usr;
    cpu->cpsr = MODE_SYS; // IRQs enabled, ARM mode

    // ROM entry point
    cpu->r[15] = 0x08000000;
    arm7tdmi_fill_pipeline_arm(cpu);

    gba->intro_completed = true;
}

// SWI Implementations
void bios_handle_swi(ARM7TDMI* cpu, uint8_t comment) {
    GBA* gba = cpu->gba;
    Bus* bus = &gba->bus;

    switch (comment) {
        case 0x00: // SoftReset
            bios_setup_post_boot(gba);
            break;

        case 0x01: { // RegisterRamReset
            uint8_t flags = (uint8_t)cpu->r[0];
            if (flags & (1 << 0)) memset(bus->ewram, 0, sizeof(bus->ewram));
            if (flags & (1 << 1)) memset(bus->iwram, 0, sizeof(bus->iwram));
            if (flags & (1 << 2)) memset(bus->pram, 0, sizeof(bus->pram));
            if (flags & (1 << 3)) memset(bus->vram, 0, sizeof(bus->vram));
            if (flags & (1 << 4)) memset(bus->oam, 0, sizeof(bus->oam));
            if (flags & (1 << 5)) memset(&bus->io[REG_SOUND1CNT_L], 0, 0x40);
            if (flags & (1 << 6)) apu_reset(&gba->apu);
            break;
        }

        case 0x02: // Halt
            gba->halted = true;
            break;

        case 0x03: // Stop
            gba->stopped = true;
            break;

        case 0x04: { // IntrWait
            uint32_t discard_old = cpu->r[0];
            uint32_t wait_mask = cpu->r[1];
            uint16_t* pflags = (uint16_t*)&bus->iwram[0x7FF8];
            if (discard_old) {
                *pflags &= ~wait_mask;
            }
            if (*pflags & wait_mask) {
                *pflags &= ~wait_mask;
                break;
            }
            gba->interrupt.ime = 1;
            gba->halted = true;
            break;
        }

        case 0x05: { // VBlankIntrWait
            uint16_t* pflags = (uint16_t*)&bus->iwram[0x7FF8];
            *pflags &= ~IRQ_VBLANK;
            gba->interrupt.ime = 1;
            gba->halted = true;
            break;
        }

        case 0x06: { // Div: R0 / R1
            int32_t num = (int32_t)cpu->r[0];
            int32_t den = (int32_t)cpu->r[1];
            if (den != 0) {
                int32_t quot = num / den;
                int32_t rem = num % den;
                cpu->r[0] = (uint32_t)quot;
                cpu->r[1] = (uint32_t)rem;
                cpu->r[3] = (uint32_t)abs(quot);
            }
            break;
        }

        case 0x07: { // DivArm: R1 / R0
            int32_t num = (int32_t)cpu->r[1];
            int32_t den = (int32_t)cpu->r[0];
            if (den != 0) {
                int32_t quot = num / den;
                int32_t rem = num % den;
                cpu->r[0] = (uint32_t)quot;
                cpu->r[1] = (uint32_t)rem;
                cpu->r[3] = (uint32_t)abs(quot);
            }
            break;
        }

        case 0x08: { // Sqrt
            uint32_t val = cpu->r[0];
            cpu->r[0] = (uint32_t)sqrt((double)val);
            break;
        }

        case 0x09: { // ArcTan
            int16_t x = (int16_t)cpu->r[0];
            double rad = atan((double)x / 16384.0);
            cpu->r[0] = (uint32_t)(int16_t)(rad * 16384.0 / (2.0 * M_PI));
            break;
        }

        case 0x0A: { // ArcTan2
            int16_t x = (int16_t)cpu->r[0];
            int16_t y = (int16_t)cpu->r[1];
            double rad = atan2((double)y, (double)x);
            cpu->r[0] = (uint16_t)(rad * 65536.0 / (2.0 * M_PI));
            break;
        }

        case 0x0B: { // CpuSet
            uint32_t src = cpu->r[0];
            uint32_t dst = cpu->r[1];
            uint32_t cnt = cpu->r[2];
            bool fixed = (cnt & (1 << 24)) != 0;
            bool is_32bit = (cnt & (1 << 26)) != 0;
            uint32_t len = cnt & 0x1FFFFF;

            if (is_32bit) {
                for (uint32_t i = 0; i < len; i++) {
                    uint32_t val = bus_read32(bus, src);
                    bus_write32(bus, dst, val);
                    if (!fixed) src += 4;
                    dst += 4;
                }
            } else {
                for (uint32_t i = 0; i < len; i++) {
                    uint16_t val = bus_read16(bus, src);
                    bus_write16(bus, dst, val);
                    if (!fixed) src += 2;
                    dst += 2;
                }
            }
            break;
        }

        case 0x0C: { // CpuFastSet
            uint32_t src = cpu->r[0];
            uint32_t dst = cpu->r[1];
            uint32_t cnt = cpu->r[2];
            bool fixed = (cnt & (1 << 24)) != 0;
            uint32_t words = (cnt & 0x1FFFFF);
            // Rounded up to multiple of 8 words
            words = (words + 7) & ~7U;

            for (uint32_t i = 0; i < words; i++) {
                uint32_t val = bus_read32(bus, src);
                bus_write32(bus, dst, val);
                if (!fixed) src += 4;
                dst += 4;
            }
            break;
        }

        case 0x0E: { // BgAffineSet
            // R0 = src struct pointer, R1 = dst struct pointer, R2 = count
            uint32_t src = cpu->r[0];
            uint32_t dst = cpu->r[1];
            uint32_t count = cpu->r[2];

            for (uint32_t i = 0; i < count; i++) {
                int32_t cx = (int32_t)bus_read32(bus, src + 0);
                int32_t cy = (int32_t)bus_read32(bus, src + 4);
                int16_t dx = (int16_t)bus_read16(bus, src + 8);
                int16_t dy = (int16_t)bus_read16(bus, src + 10);
                int16_t sx = (int16_t)bus_read16(bus, src + 12);
                int16_t sy = (int16_t)bus_read16(bus, src + 14);
                uint16_t angle = bus_read16(bus, src + 16);
                src += 20;

                double rad = (double)angle * (2.0 * M_PI / 65536.0);
                double cos_a = cos(rad);
                double sin_a = sin(rad);

                int16_t pa = (int16_t)((cos_a * 256.0 * 256.0) / sx);
                int16_t pb = (int16_t)((-sin_a * 256.0 * 256.0) / sx);
                int16_t pc = (int16_t)((sin_a * 256.0 * 256.0) / sy);
                int16_t pd = (int16_t)((cos_a * 256.0 * 256.0) / sy);

                bus_write16(bus, dst + 0, (uint16_t)pa);
                bus_write16(bus, dst + 2, (uint16_t)pb);
                bus_write16(bus, dst + 4, (uint16_t)pc);
                bus_write16(bus, dst + 6, (uint16_t)pd);

                int32_t start_x = cx - (pa * dx + pb * dy);
                int32_t start_y = cy - (pc * dx + pd * dy);
                bus_write32(bus, dst + 8, (uint32_t)start_x);
                bus_write32(bus, dst + 12, (uint32_t)start_y);

                dst += 16;
            }
            break;
        }

        case 0x0F: { // ObjAffineSet
            uint32_t src = cpu->r[0];
            uint32_t dst = cpu->r[1];
            uint32_t count = cpu->r[2];
            uint32_t stride = cpu->r[3];

            for (uint32_t i = 0; i < count; i++) {
                int16_t sx = (int16_t)bus_read16(bus, src + 0);
                int16_t sy = (int16_t)bus_read16(bus, src + 2);
                uint16_t angle = bus_read16(bus, src + 4);
                src += 8;

                double rad = (double)angle * (2.0 * M_PI / 65536.0);
                double cos_a = cos(rad);
                double sin_a = sin(rad);

                int16_t pa = (int16_t)((cos_a * 256.0 * 256.0) / (sx != 0 ? sx : 1));
                int16_t pb = (int16_t)((-sin_a * 256.0 * 256.0) / (sx != 0 ? sx : 1));
                int16_t pc = (int16_t)((sin_a * 256.0 * 256.0) / (sy != 0 ? sy : 1));
                int16_t pd = (int16_t)((cos_a * 256.0 * 256.0) / (sy != 0 ? sy : 1));

                bus_write16(bus, dst + 0, (uint16_t)pa);
                bus_write16(bus, dst + stride, (uint16_t)pb);
                bus_write16(bus, dst + stride * 2, (uint16_t)pc);
                bus_write16(bus, dst + stride * 3, (uint16_t)pd);

                dst += stride * 4;
            }
            break;
        }

        case 0x10: { // BitUnPack
            uint32_t src = cpu->r[0];
            uint32_t dst = cpu->r[1];
            uint32_t bup = cpu->r[2];

            uint16_t src_len = bus_read16(bus, bup + 0);
            uint8_t src_width = bus_read8(bus, bup + 2);
            uint8_t dst_width = bus_read8(bus, bup + 3);
            uint32_t data_offset = bus_read32(bus, bup + 4);
            bool zero_flag = (data_offset & (1U << 31)) != 0;
            uint32_t offset_val = data_offset & 0x7FFFFFFF;

            if (src_width > 0 && dst_width > 0) {
                uint32_t src_bits = 0;
                uint32_t src_cache = 0;
                uint32_t dst_bits = 0;
                uint32_t dst_cache = 0;
                uint32_t total_src_bits = (uint32_t)src_len * 8;
                uint32_t processed = 0;

                while (processed < total_src_bits) {
                    while (src_bits < src_width && processed + src_bits < total_src_bits) {
                        src_cache |= ((uint32_t)bus_read8(bus, src++) << src_bits);
                        src_bits += 8;
                    }
                    uint32_t val = src_cache & ((1U << src_width) - 1);
                    src_cache >>= src_width;
                    src_bits = (src_bits >= src_width) ? (src_bits - src_width) : 0;
                    processed += src_width;

                    if (val == 0 && !zero_flag) {
                        // zero stays zero
                    } else {
                        val += offset_val;
                    }

                    dst_cache |= (val & ((1U << dst_width) - 1)) << dst_bits;
                    dst_bits += dst_width;

                    while (dst_bits >= 32) {
                        bus_write32(bus, dst, dst_cache);
                        dst += 4;
                        dst_cache = 0;
                        dst_bits -= 32;
                    }
                }
                if (dst_bits > 0) {
                    bus_write32(bus, dst, dst_cache);
                }
            }
            break;
        }

        case 0x11: // LZ77UnCompWram
        case 0x12: { // LZ77UnCompVram
            uint32_t src = cpu->r[0];
            uint32_t dst = cpu->r[1];
            uint32_t header = bus_read32(bus, src);
            src += 4;
            uint32_t uncomp_len = header >> 8;
            if (uncomp_len > 0x100000) uncomp_len = 0x100000; // 1MB safety cap

            uint8_t* out = (uint8_t*)malloc(uncomp_len ? uncomp_len : 1);
            if (!out) break;

            uint32_t written = 0;
            while (written < uncomp_len) {
                uint8_t flag_byte = bus_read8(bus, src++);
                for (int bit = 7; bit >= 0 && written < uncomp_len; bit--) {
                    if (flag_byte & (1 << bit)) {
                        uint8_t b1 = bus_read8(bus, src++);
                        uint8_t b2 = bus_read8(bus, src++);
                        uint32_t length = (b1 >> 4) + 3;
                        uint32_t disp = (((b1 & 0xF) << 8) | b2) + 1;
                        for (uint32_t k = 0; k < length && written < uncomp_len; k++) {
                            uint8_t byte = (written >= disp) ? out[written - disp] : 0;
                            out[written++] = byte;
                        }
                    } else {
                        uint8_t byte = bus_read8(bus, src++);
                        out[written++] = byte;
                    }
                }
            }

            uint8_t dst_region = (dst >> 24) & 0xFF;
            if (dst_region == 0x06) {
                for (uint32_t i = 0; i < written; i += 2) {
                    uint16_t hw = out[i];
                    if (i + 1 < written) hw |= ((uint16_t)out[i + 1] << 8);
                    bus_write16(bus, dst + i, hw);
                }
            } else {
                for (uint32_t i = 0; i < written; i++) {
                    bus_write8(bus, dst + i, out[i]);
                }
            }
            free(out);
            break;
        }

        case 0x14: // RLUnCompWram
        case 0x15: { // RLUnCompVram
            uint32_t src = cpu->r[0];
            uint32_t dst = cpu->r[1];
            uint32_t header = bus_read32(bus, src);
            src += 4;
            uint32_t uncomp_len = header >> 8;
            if (uncomp_len > 0x100000) uncomp_len = 0x100000;

            uint8_t* out = (uint8_t*)malloc(uncomp_len ? uncomp_len : 1);
            if (!out) break;

            uint32_t written = 0;
            while (written < uncomp_len) {
                uint8_t flag_byte = bus_read8(bus, src++);
                bool compressed = (flag_byte & 0x80) != 0;
                uint32_t length = (flag_byte & 0x7F) + (compressed ? 3 : 1);

                if (compressed) {
                    uint8_t val = bus_read8(bus, src++);
                    for (uint32_t k = 0; k < length && written < uncomp_len; k++) {
                        out[written++] = val;
                    }
                } else {
                    for (uint32_t k = 0; k < length && written < uncomp_len; k++) {
                        uint8_t val = bus_read8(bus, src++);
                        out[written++] = val;
                    }
                }
            }

            uint8_t dst_region = (dst >> 24) & 0xFF;
            if (dst_region == 0x06) {
                for (uint32_t i = 0; i < written; i += 2) {
                    uint16_t hw = out[i];
                    if (i + 1 < written) hw |= ((uint16_t)out[i + 1] << 8);
                    bus_write16(bus, dst + i, hw);
                }
            } else {
                for (uint32_t i = 0; i < written; i++) {
                    bus_write8(bus, dst + i, out[i]);
                }
            }
            free(out);
            break;
        }

        case 0x19: { // SoundBias
            bus_write16(bus, REG_SOUNDBIAS, (uint16_t)cpu->r[0]);
            break;
        }

        default:
            break;
    }
}
