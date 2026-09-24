#include "ppu.h"
#include "gba.h"
#include "bus.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const int sprite_sizes[3][4][2] = {
    // Shape 0: Square
    { {8, 8}, {16, 16}, {32, 32}, {64, 64} },
    // Shape 1: Horizontal (Wide)
    { {16, 8}, {32, 8}, {32, 16}, {64, 32} },
    // Shape 2: Vertical (Tall)
    { {8, 16}, {8, 32}, {16, 32}, {32, 64} }
};

void ppu_init(PPU* ppu, GBA* gba) {
    memset(ppu, 0, sizeof(PPU));
    ppu->gba = gba;
}

void ppu_reset(PPU* ppu) {
    ppu->dispcnt = 0x0080; // Forced blank default
    ppu->dispstat = 0;
    ppu->vcount = 0;
    memset(ppu->bgcnt, 0, sizeof(ppu->bgcnt));
    memset(ppu->bghofs, 0, sizeof(ppu->bghofs));
    memset(ppu->bgvofs, 0, sizeof(ppu->bgvofs));
    ppu->bgpa[0] = 0x0100; ppu->bgpd[0] = 0x0100;
    ppu->bgpa[1] = 0x0100; ppu->bgpd[1] = 0x0100;
}

uint16_t ppu_read16(PPU* ppu, uint32_t addr) {
    uint32_t offset = addr & 0x3FF;
    switch (offset) {
        case REG_DISPCNT:  return ppu->dispcnt;
        case REG_DISPSTAT: return ppu->dispstat;
        case REG_VCOUNT:   return ppu->vcount;
        case REG_BG0CNT:   return ppu->bgcnt[0];
        case REG_BG1CNT:   return ppu->bgcnt[1];
        case REG_BG2CNT:   return ppu->bgcnt[2];
        case REG_BG3CNT:   return ppu->bgcnt[3];
        case REG_WIN0H:    return ppu->win0h;
        case REG_WIN1H:    return ppu->win1h;
        case REG_WIN0V:    return ppu->win0v;
        case REG_WIN1V:    return ppu->win1v;
        case REG_WININ:    return ppu->winin;
        case REG_WINOUT:   return ppu->winout;
        case REG_BLDCNT:   return ppu->bldcnt;
        case REG_BLDALPHA: return ppu->bldalpha;
        default:           return *(uint16_t*)&ppu->gba->bus.io[offset];
    }
}

void ppu_write16(PPU* ppu, uint32_t addr, uint16_t val) {
    uint32_t offset = addr & 0x3FF;
    switch (offset) {
        case REG_DISPCNT:  ppu->dispcnt = val; break;
        case REG_DISPSTAT: ppu->dispstat = (ppu->dispstat & 0x07) | (val & ~0x07); break;
        case REG_BG0CNT:   ppu->bgcnt[0] = val; break;
        case REG_BG1CNT:   ppu->bgcnt[1] = val; break;
        case REG_BG2CNT:   ppu->bgcnt[2] = val; break;
        case REG_BG3CNT:   ppu->bgcnt[3] = val; break;
        case REG_BG0HOFS:  ppu->bghofs[0] = val & 0x1FF; break;
        case REG_BG0VOFS:  ppu->bgvofs[0] = val & 0x1FF; break;
        case REG_BG1HOFS:  ppu->bghofs[1] = val & 0x1FF; break;
        case REG_BG1VOFS:  ppu->bgvofs[1] = val & 0x1FF; break;
        case REG_BG2HOFS:  ppu->bghofs[2] = val & 0x1FF; break;
        case REG_BG2VOFS:  ppu->bgvofs[2] = val & 0x1FF; break;
        case REG_BG3HOFS:  ppu->bghofs[3] = val & 0x1FF; break;
        case REG_BG3VOFS:  ppu->bgvofs[3] = val & 0x1FF; break;
        case REG_BG2PA:    ppu->bgpa[0] = (int16_t)val; break;
        case REG_BG2PB:    ppu->bgpb[0] = (int16_t)val; break;
        case REG_BG2PC:    ppu->bgpc[0] = (int16_t)val; break;
        case REG_BG2PD:    ppu->bgpd[0] = (int16_t)val; break;
        case REG_BG2X_L:   ppu->bgx[0] = (ppu->bgx[0] & (int32_t)0xFFFF0000) | val; ppu->bgx_internal[0] = ppu->bgx[0]; break;
        case REG_BG2X_H: {
            int32_t hi = (int16_t)val;
            ppu->bgx[0] = (ppu->bgx[0] & 0x0000FFFF) | (hi << 16);
            if (ppu->bgx[0] & 0x08000000) ppu->bgx[0] |= 0xF0000000;
            ppu->bgx_internal[0] = ppu->bgx[0];
            break;
        }
        case REG_BG2Y_L:   ppu->bgy[0] = (ppu->bgy[0] & (int32_t)0xFFFF0000) | val; ppu->bgy_internal[0] = ppu->bgy[0]; break;
        case REG_BG2Y_H: {
            int32_t hi = (int16_t)val;
            ppu->bgy[0] = (ppu->bgy[0] & 0x0000FFFF) | (hi << 16);
            if (ppu->bgy[0] & 0x08000000) ppu->bgy[0] |= 0xF0000000;
            ppu->bgy_internal[0] = ppu->bgy[0];
            break;
        }
        case REG_BG3PA:    ppu->bgpa[1] = (int16_t)val; break;
        case REG_BG3PB:    ppu->bgpb[1] = (int16_t)val; break;
        case REG_BG3PC:    ppu->bgpc[1] = (int16_t)val; break;
        case REG_BG3PD:    ppu->bgpd[1] = (int16_t)val; break;
        case REG_BG3X_L:   ppu->bgx[1] = (ppu->bgx[1] & (int32_t)0xFFFF0000) | val; ppu->bgx_internal[1] = ppu->bgx[1]; break;
        case REG_BG3X_H: {
            int32_t hi = (int16_t)val;
            ppu->bgx[1] = (ppu->bgx[1] & 0x0000FFFF) | (hi << 16);
            if (ppu->bgx[1] & 0x08000000) ppu->bgx[1] |= 0xF0000000;
            ppu->bgx_internal[1] = ppu->bgx[1];
            break;
        }
        case REG_BG3Y_L:   ppu->bgy[1] = (ppu->bgy[1] & (int32_t)0xFFFF0000) | val; ppu->bgy_internal[1] = ppu->bgy[1]; break;
        case REG_BG3Y_H: {
            int32_t hi = (int16_t)val;
            ppu->bgy[1] = (ppu->bgy[1] & 0x0000FFFF) | (hi << 16);
            if (ppu->bgy[1] & 0x08000000) ppu->bgy[1] |= 0xF0000000;
            ppu->bgy_internal[1] = ppu->bgy[1];
            break;
        }
        case REG_WIN0H:    ppu->win0h = val; break;
        case REG_WIN1H:    ppu->win1h = val; break;
        case REG_WIN0V:    ppu->win0v = val; break;
        case REG_WIN1V:    ppu->win1v = val; break;
        case REG_WININ:    ppu->winin = val; break;
        case REG_WINOUT:   ppu->winout = val; break;
        case REG_MOSAIC:   ppu->mosaic = val; break;
        case REG_BLDCNT:   ppu->bldcnt = val; break;
        case REG_BLDALPHA: ppu->bldalpha = val; break;
        case REG_BLDY:     ppu->bldy = val; break;
        default:           break;
    }
}

// Convert 15-bit GBA BGR555 to host 32-bit RGBA (0xAABBGGRR)
static inline uint32_t rgb555_to_rgba32(uint16_t color) {
    uint8_t r = (color & 0x1F) << 3;
    uint8_t g = ((color >> 5) & 0x1F) << 3;
    uint8_t b = ((color >> 10) & 0x1F) << 3;
    r |= (r >> 5);
    g |= (g >> 5);
    b |= (b >> 5);
    return 0xFF000000 | ((uint32_t)r) | ((uint32_t)g << 8) | ((uint32_t)b << 16);
}

// Color blend / special effects
static inline uint16_t blend_colors(uint16_t top, uint16_t bot, uint8_t eva, uint8_t evb) {
    uint32_t r1 = top & 0x1F, g1 = (top >> 5) & 0x1F, b1 = (top >> 10) & 0x1F;
    uint32_t r2 = bot & 0x1F, g2 = (bot >> 5) & 0x1F, b2 = (bot >> 10) & 0x1F;

    uint32_t r = (r1 * eva + r2 * evb) / 16; if (r > 31) r = 31;
    uint32_t g = (g1 * eva + g2 * evb) / 16; if (g > 31) g = 31;
    uint32_t b = (b1 * eva + b2 * evb) / 16; if (b > 31) b = 31;

    return (uint16_t)(r | (g << 5) | (b << 10));
}

static inline uint16_t brighten_color(uint16_t col, uint8_t evy) {
    uint32_t r = col & 0x1F, g = (col >> 5) & 0x1F, b = (col >> 10) & 0x1F;
    r += (31 - r) * evy / 16; if (r > 31) r = 31;
    g += (31 - g) * evy / 16; if (g > 31) g = 31;
    b += (31 - b) * evy / 16; if (b > 31) b = 31;
    return (uint16_t)(r | (g << 5) | (b << 10));
}

static inline uint16_t darken_color(uint16_t col, uint8_t evy) {
    uint32_t r = col & 0x1F, g = (col >> 5) & 0x1F, b = (col >> 10) & 0x1F;
    r -= r * evy / 16;
    g -= g * evy / 16;
    b -= b * evy / 16;
    return (uint16_t)(r | (g << 5) | (b << 10));
}

// Render Text Background Line
static void render_text_bg(PPU* ppu, int bg_idx, int line) {
    uint16_t cnt = ppu->bgcnt[bg_idx];
    uint8_t priority = cnt & 0x3;
    uint32_t char_base = ((cnt >> 2) & 0x3) * 0x4000;
    uint32_t screen_base = ((cnt >> 8) & 0x1F) * 0x800;
    bool is_8bpp = (cnt & (1 << 7)) != 0;
    uint8_t screen_size = (cnt >> 14) & 0x3;

    int scroll_x = ppu->bghofs[bg_idx];
    int scroll_y = ppu->bgvofs[bg_idx];
    int y = (scroll_y + line) & (screen_size >= 2 ? 511 : 255);

    uint8_t* vram = ppu->gba->bus.vram;
    uint16_t* pram = (uint16_t*)ppu->gba->bus.pram;

    for (int x_screen = 0; x_screen < 240; x_screen++) {
        int x = (scroll_x + x_screen) & (screen_size == 1 || screen_size == 3 ? 511 : 255);

        int tile_x = x / 8;
        int tile_y = y / 8;
        int fine_x = x % 8;
        int fine_y = y % 8;

        // Calculate screen block index based on screen size
        uint32_t cur_screen_base = screen_base;
        if (screen_size == 1) { // 64x32
            if (tile_x >= 32) { cur_screen_base += 0x800; tile_x -= 32; }
        } else if (screen_size == 2) { // 32x64
            if (tile_y >= 32) { cur_screen_base += 0x800; tile_y -= 32; }
        } else if (screen_size == 3) { // 64x64
            if (tile_x >= 32) { cur_screen_base += 0x800; tile_x -= 32; }
            if (tile_y >= 32) { cur_screen_base += 0x1000; tile_y -= 32; }
        }

        uint32_t map_entry_addr = cur_screen_base + (tile_y * 32 + tile_x) * 2;
        if (map_entry_addr + 1 >= VRAM_SIZE) continue;
        uint16_t map_entry = *(uint16_t*)&vram[map_entry_addr];

        uint16_t tile_idx = map_entry & 0x3FF;
        bool h_flip = (map_entry & (1 << 10)) != 0;
        bool v_flip = (map_entry & (1 << 11)) != 0;
        uint8_t pal_bank = (map_entry >> 12) & 0xF;

        int px = h_flip ? (7 - fine_x) : fine_x;
        int py = v_flip ? (7 - fine_y) : fine_y;

        uint16_t color = 0;
        bool visible = false;

        if (is_8bpp) {
            uint32_t tile_data_addr = char_base + tile_idx * 64 + py * 8 + px;
            if (tile_data_addr < VRAM_SIZE) {
                uint8_t pal_entry = vram[tile_data_addr];
                if (pal_entry != 0) {
                    color = pram[pal_entry];
                    visible = true;
                }
            }
        } else {
            uint32_t tile_data_addr = char_base + tile_idx * 32 + py * 4 + (px / 2);
            if (tile_data_addr < VRAM_SIZE) {
                uint8_t byte = vram[tile_data_addr];
                uint8_t pal_entry = (px & 1) ? (byte >> 4) : (byte & 0xF);
                if (pal_entry != 0) {
                    color = pram[pal_bank * 16 + pal_entry];
                    visible = true;
                }
            }
        }

        if (visible) {
            ppu->bg_scanline[bg_idx][x_screen] = color | ((uint32_t)bg_idx << 16) | ((uint32_t)priority << 20);
            ppu->bg_scanline_valid[bg_idx][x_screen] = true;
        }
    }
}

// Render Affine Background Line (Mode 1 / 2)
static void render_affine_bg(PPU* ppu, int bg_idx, int line) {
    (void)line;
    int aff_idx = bg_idx - 2;
    uint16_t cnt = ppu->bgcnt[bg_idx];
    uint8_t priority = cnt & 0x3;
    uint32_t char_base = ((cnt >> 2) & 0x3) * 0x4000;
    uint32_t screen_base = ((cnt >> 8) & 0x1F) * 0x800;
    bool wrap = (cnt & (1 << 13)) != 0;
    uint8_t screen_size = (cnt >> 14) & 0x3;
    int size = 128 << screen_size; // 128, 256, 512, 1024

    int32_t x_ref = ppu->bgx_internal[aff_idx];
    int32_t y_ref = ppu->bgy_internal[aff_idx];
    int16_t pa = ppu->bgpa[aff_idx];
    int16_t pc = ppu->bgpc[aff_idx];

    uint8_t* vram = ppu->gba->bus.vram;
    uint16_t* pram = (uint16_t*)ppu->gba->bus.pram;

    for (int x_screen = 0; x_screen < 240; x_screen++) {
        int32_t cur_x = (x_ref + pa * x_screen) >> 8;
        int32_t cur_y = (y_ref + pc * x_screen) >> 8;

        if (wrap) {
            cur_x = (cur_x % size + size) % size;
            cur_y = (cur_y % size + size) % size;
        } else {
            if (cur_x < 0 || cur_x >= size || cur_y < 0 || cur_y >= size) {
                continue;
            }
        }

        int tile_x = cur_x / 8;
        int tile_y = cur_y / 8;
        int fine_x = cur_x % 8;
        int fine_y = cur_y % 8;

        int tiles_per_row = size / 8;
        uint32_t map_entry_addr = screen_base + tile_y * tiles_per_row + tile_x;
        if (map_entry_addr >= VRAM_SIZE) continue;
        uint8_t tile_idx = vram[map_entry_addr];

        uint32_t tile_data_addr = char_base + tile_idx * 64 + fine_y * 8 + fine_x;
        if (tile_data_addr >= VRAM_SIZE) continue;
        uint8_t pal_entry = vram[tile_data_addr];

        if (pal_entry != 0) {
            uint16_t color = pram[pal_entry];
            ppu->bg_scanline[bg_idx][x_screen] = color | ((uint32_t)bg_idx << 16) | ((uint32_t)priority << 20);
            ppu->bg_scanline_valid[bg_idx][x_screen] = true;
        }
    }
}

// Render Bitmap Modes (3, 4, 5)
static void render_bitmap_bg(PPU* ppu, int mode, int line) {
    uint8_t* vram = ppu->gba->bus.vram;
    uint16_t* pram = (uint16_t*)ppu->gba->bus.pram;
    uint8_t priority = ppu->bgcnt[2] & 0x3;
    bool page = (ppu->dispcnt & (1 << 4)) != 0;

    if (mode == 3) { // 240x160 16-bit RGB555
        uint32_t line_addr = line * 240 * 2;
        for (int x = 0; x < 240; x++) {
            uint16_t color = *(uint16_t*)&vram[line_addr + x * 2];
            ppu->bg_scanline[2][x] = color | (2U << 16) | ((uint32_t)priority << 20);
            ppu->bg_scanline_valid[2][x] = true;
        }
    } else if (mode == 4) { // 240x160 8-bit paletted, double buffered
        uint32_t line_addr = (page ? 0xA000 : 0x0000) + line * 240;
        for (int x = 0; x < 240; x++) {
            uint8_t pal_entry = vram[line_addr + x];
            if (pal_entry != 0) {
                uint16_t color = pram[pal_entry];
                ppu->bg_scanline[2][x] = color | (2U << 16) | ((uint32_t)priority << 20);
                ppu->bg_scanline_valid[2][x] = true;
            }
        }
    } else if (mode == 5) { // 160x128 16-bit direct color, double buffered
        if (line < 128) {
            uint32_t line_addr = (page ? 0xA000 : 0x0000) + line * 160 * 2;
            for (int x = 0; x < 160; x++) {
                uint16_t color = *(uint16_t*)&vram[line_addr + x * 2];
                ppu->bg_scanline[2][x] = color | (2U << 16) | ((uint32_t)priority << 20);
                ppu->bg_scanline_valid[2][x] = true;
            }
        }
    }
}

// Render Sprites / OAM
static void render_sprites(PPU* ppu, int line) {
    if (!(ppu->dispcnt & (1 << 12))) return; // OBJ disabled

    uint16_t* oam = (uint16_t*)ppu->gba->bus.oam;
    uint8_t* vram = ppu->gba->bus.vram;
    uint16_t* pram = (uint16_t*)ppu->gba->bus.pram;
    bool mapping_1d = (ppu->dispcnt & (1 << 6)) != 0;

    // Evaluate up to 128 sprites in reverse priority (sprite 127 down to 0)
    for (int i = 127; i >= 0; i--) {
        uint16_t attr0 = oam[i * 4 + 0];
        uint16_t attr1 = oam[i * 4 + 1];
        uint16_t attr2 = oam[i * 4 + 2];

        bool is_affine = (attr0 & (1 << 8)) != 0;
        bool double_or_disable = (attr0 & (1 << 9)) != 0;
        if (!is_affine && double_or_disable) {
            continue; // Disabled
        }

        uint8_t mode = (attr0 >> 10) & 0x3;
        bool is_8bpp = (attr0 & (1 << 13)) != 0;
        uint8_t shape = (attr0 >> 14) & 0x3;
        if (shape == 3) continue;

        uint8_t size_idx = (attr1 >> 14) & 0x3;
        int width = sprite_sizes[shape][size_idx][0];
        int height = sprite_sizes[shape][size_idx][1];

        int y_coord = attr0 & 0xFF;
        if (y_coord >= 160) y_coord -= 256;

        int bounding_w = width;
        int bounding_h = height;
        if (is_affine && double_or_disable) {
            bounding_w *= 2;
            bounding_h *= 2;
        }

        if (line < y_coord || line >= y_coord + bounding_h) {
            continue; // Outside current scanline
        }

        int x_coord = attr1 & 0x1FF;
        if (x_coord >= 240) x_coord -= 512;

        uint16_t tile_idx = attr2 & 0x3FF;
        uint8_t priority = (attr2 >> 10) & 0x3;
        uint8_t pal_bank = (attr2 >> 12) & 0xF;

        if (!is_affine) {
            bool h_flip = (attr1 & (1 << 12)) != 0;
            bool v_flip = (attr1 & (1 << 13)) != 0;
            int fine_y = line - y_coord;
            int sprite_y = v_flip ? (height - 1 - fine_y) : fine_y;

            for (int px = 0; px < width; px++) {
                int screen_x = x_coord + px;
                if (screen_x < 0 || screen_x >= 240) continue;

                int sprite_x = h_flip ? (width - 1 - px) : px;
                int tile_x = sprite_x / 8;
                int tile_y = sprite_y / 8;
                int in_tile_x = sprite_x % 8;
                int in_tile_y = sprite_y % 8;

                uint32_t cur_tile = mapping_1d ? 
                    (tile_idx + (tile_y * (width / 8) + tile_x) * (is_8bpp ? 2 : 1)) :
                    (tile_idx + tile_y * 32 + tile_x * (is_8bpp ? 2 : 1));

                uint32_t tile_addr = 0x10000 + cur_tile * 32;
                uint16_t color = 0;
                bool visible = false;

                if (is_8bpp) {
                    uint32_t px_addr = tile_addr + in_tile_y * 8 + in_tile_x;
                    if (px_addr < VRAM_SIZE) {
                        uint8_t pal_entry = vram[px_addr];
                        if (pal_entry != 0) {
                            color = pram[256 + pal_entry];
                            visible = true;
                        }
                    }
                } else {
                    uint32_t px_addr = tile_addr + in_tile_y * 4 + in_tile_x / 2;
                    if (px_addr < VRAM_SIZE) {
                        uint8_t byte = vram[px_addr];
                        uint8_t pal_entry = (in_tile_x & 1) ? (byte >> 4) : (byte & 0xF);
                        if (pal_entry != 0) {
                            color = pram[256 + pal_bank * 16 + pal_entry];
                            visible = true;
                        }
                    }
                }

                if (visible) {
                    if (mode == 2) { // OBJ Window
                        ppu->obj_window_mask[screen_x] = true;
                    } else {
                        // Check priority over existing OBJ pixels (lower OAM index wins ties)
                        bool higher_prio = true;
                        if (ppu->obj_scanline_valid[screen_x]) {
                            uint8_t existing_prio = (ppu->obj_scanline[screen_x] >> 20) & 0x3;
                            if (priority > existing_prio) higher_prio = false;
                        }
                        if (higher_prio) {
                            uint32_t pixel = color | (4U << 16) | ((uint32_t)priority << 20);
                            if (mode == 1) pixel |= (1U << 22); // Semi-transparent
                            ppu->obj_scanline[screen_x] = pixel;
                            ppu->obj_scanline_valid[screen_x] = true;
                        }
                    }
                }
            }
        } else {
            // Affine sprite rendering
            int aff_group = (attr1 >> 9) & 0x1F;
            int16_t pa = (int16_t)oam[aff_group * 16 + 3];
            int16_t pb = (int16_t)oam[aff_group * 16 + 7];
            int16_t pc = (int16_t)oam[aff_group * 16 + 11];
            int16_t pd = (int16_t)oam[aff_group * 16 + 15];

            int cx = bounding_w / 2;
            int cy = bounding_h / 2;
            int fine_y = line - y_coord;

            for (int px = 0; px < bounding_w; px++) {
                int screen_x = x_coord + px;
                if (screen_x < 0 || screen_x >= 240) continue;

                int dx = px - cx;
                int dy = fine_y - cy;

                int sprite_x = (pa * dx + pb * dy) >> 8;
                int sprite_y = (pc * dx + pd * dy) >> 8;

                sprite_x += width / 2;
                sprite_y += height / 2;

                if (sprite_x < 0 || sprite_x >= width || sprite_y < 0 || sprite_y >= height) {
                    continue;
                }

                int tile_x = sprite_x / 8;
                int tile_y = sprite_y / 8;
                int in_tile_x = sprite_x % 8;
                int in_tile_y = sprite_y % 8;

                uint32_t cur_tile = mapping_1d ? 
                    (tile_idx + (tile_y * (width / 8) + tile_x) * (is_8bpp ? 2 : 1)) :
                    (tile_idx + tile_y * 32 + tile_x * (is_8bpp ? 2 : 1));

                uint32_t tile_addr = 0x10000 + cur_tile * 32;
                uint16_t color = 0;
                bool visible = false;

                if (is_8bpp) {
                    uint32_t px_addr = tile_addr + in_tile_y * 8 + in_tile_x;
                    if (px_addr < VRAM_SIZE) {
                        uint8_t pal_entry = vram[px_addr];
                        if (pal_entry != 0) {
                            color = pram[256 + pal_entry];
                            visible = true;
                        }
                    }
                } else {
                    uint32_t px_addr = tile_addr + in_tile_y * 4 + in_tile_x / 2;
                    if (px_addr < VRAM_SIZE) {
                        uint8_t byte = vram[px_addr];
                        uint8_t pal_entry = (in_tile_x & 1) ? (byte >> 4) : (byte & 0xF);
                        if (pal_entry != 0) {
                            color = pram[256 + pal_bank * 16 + pal_entry];
                            visible = true;
                        }
                    }
                }

                if (visible) {
                    if (mode == 2) {
                        ppu->obj_window_mask[screen_x] = true;
                    } else {
                        // Check priority over existing OBJ pixels (lower OAM index wins ties)
                        bool higher_prio = true;
                        if (ppu->obj_scanline_valid[screen_x]) {
                            uint8_t existing_prio = (ppu->obj_scanline[screen_x] >> 20) & 0x3;
                            if (priority > existing_prio) higher_prio = false;
                        }
                        if (higher_prio) {
                            uint32_t pixel = color | (4U << 16) | ((uint32_t)priority << 20);
                            if (mode == 1) pixel |= (1U << 22);
                            ppu->obj_scanline[screen_x] = pixel;
                            ppu->obj_scanline_valid[screen_x] = true;
                        }
                    }
                }
            }
        }
    }
}

// Window mask checking for a specific pixel
static inline uint8_t get_active_window(PPU* ppu, int x, int line) {
    bool win0_en = (ppu->dispcnt & (1 << 13)) != 0;
    bool win1_en = (ppu->dispcnt & (1 << 14)) != 0;
    bool objwin_en = (ppu->dispcnt & (1 << 15)) != 0;

    if (!win0_en && !win1_en && !objwin_en) {
        return 0xFF; // No windowing active
    }

    if (win0_en) {
        int x1 = (ppu->win0h >> 8) & 0xFF, x2 = ppu->win0h & 0xFF;
        int y1 = (ppu->win0v >> 8) & 0xFF, y2 = ppu->win0v & 0xFF;
        bool in_x = (x1 <= x2) ? (x >= x1 && x < x2) : (x >= x1 || x < x2);
        bool in_y = (y1 <= y2) ? (line >= y1 && line < y2) : (line >= y1 || line < y2);
        if (in_x && in_y) return 0; // Window 0
    }

    if (win1_en) {
        int x1 = (ppu->win1h >> 8) & 0xFF, x2 = ppu->win1h & 0xFF;
        int y1 = (ppu->win1v >> 8) & 0xFF, y2 = ppu->win1v & 0xFF;
        bool in_x = (x1 <= x2) ? (x >= x1 && x < x2) : (x >= x1 || x < x2);
        bool in_y = (y1 <= y2) ? (line >= y1 && line < y2) : (line >= y1 || line < y2);
        if (in_x && in_y) return 1; // Window 1
    }

    if (objwin_en && ppu->obj_window_mask[x]) {
        return 2; // OBJ Window
    }

    return 3; // Window Out
}

// Compositor & Color Effects
void ppu_render_scanline(PPU* ppu, int line) {
    if (line >= 160) return;

    // Reset scanline buffers
    memset(ppu->bg_scanline_valid, 0, sizeof(ppu->bg_scanline_valid));
    memset(ppu->obj_scanline_valid, 0, sizeof(ppu->obj_scanline_valid));
    memset(ppu->obj_window_mask, 0, sizeof(ppu->obj_window_mask));

    if (ppu->dispcnt & (1 << 7)) { // Forced Blank
        for (int x = 0; x < 240; x++) {
            ppu->gba->framebuffer[line * 240 + x] = 0xFFFFFFFF; // White
        }
        return;
    }

    int mode = ppu->dispcnt & 0x7;

    // Render enabled backgrounds according to video mode
    if (mode == 0) {
        for (int bg = 0; bg < 4; bg++) {
            if (ppu->dispcnt & (1 << (8 + bg))) {
                render_text_bg(ppu, bg, line);
            }
        }
    } else if (mode == 1) {
        if (ppu->dispcnt & (1 << 8)) render_text_bg(ppu, 0, line);
        if (ppu->dispcnt & (1 << 9)) render_text_bg(ppu, 1, line);
        if (ppu->dispcnt & (1 << 10)) render_affine_bg(ppu, 2, line);
    } else if (mode == 2) {
        if (ppu->dispcnt & (1 << 10)) render_affine_bg(ppu, 2, line);
        if (ppu->dispcnt & (1 << 11)) render_affine_bg(ppu, 3, line);
    } else if (mode >= 3 && mode <= 5) {
        if (ppu->dispcnt & (1 << 10)) {
            render_bitmap_bg(ppu, mode, line);
        }
    }

    // Render sprites
    render_sprites(ppu, line);

    // Backdrop color (Palette RAM index 0)
    uint16_t backdrop_color = *(uint16_t*)&ppu->gba->bus.pram[0];

    // Blending registers
    uint8_t blend_mode = (ppu->bldcnt >> 6) & 0x3;
    uint8_t eva = ppu->bldalpha & 0x1F; if (eva > 16) eva = 16;
    uint8_t evb = (ppu->bldalpha >> 8) & 0x1F; if (evb > 16) evb = 16;
    uint8_t evy = ppu->bldy & 0x1F; if (evy > 16) evy = 16;

    // Composite each pixel across layers
    for (int x = 0; x < 240; x++) {
        uint8_t win_id = get_active_window(ppu, x, line);
        uint8_t win_flags = 0x3F; // Default all enabled

        if (win_id == 0) win_flags = ppu->winin & 0x3F;
        else if (win_id == 1) win_flags = (ppu->winin >> 8) & 0x3F;
        else if (win_id == 2) win_flags = (ppu->winout >> 8) & 0x3F;
        else if (win_id == 3) win_flags = ppu->winout & 0x3F;

        // Collect visible pixels for this scanline at (x, line) sorted by priority
        // Layers: 0=BG0, 1=BG1, 2=BG2, 3=BG3, 4=OBJ, 5=Backdrop
        uint32_t top_pixel = 0;
        uint32_t bot_pixel = 0;
        int top_layer = 5;
        int bot_layer = 5;
        uint16_t top_color = backdrop_color;
        uint16_t bot_color = backdrop_color;
        bool has_top = false;
        bool has_bot = false;
        bool semi_transparent_obj = false;

        // Check priorities from 0 to 3: OBJ > BG0 > BG1 > BG2 > BG3
        for (int prio = 0; prio <= 3; prio++) {
            // Check OBJ first (OBJ has higher priority than BG at the same priority level)
            if ((win_flags & (1 << 4)) && ppu->obj_scanline_valid[x]) {
                uint8_t p = (ppu->obj_scanline[x] >> 20) & 0x3;
                if (p == prio) {
                    if (!has_top) {
                        top_pixel = ppu->obj_scanline[x];
                        top_color = top_pixel & 0x7FFF;
                        top_layer = 4;
                        has_top = true;
                        if (top_pixel & (1U << 22)) semi_transparent_obj = true;
                    } else if (!has_bot) {
                        bot_pixel = ppu->obj_scanline[x];
                        bot_color = bot_pixel & 0x7FFF;
                        bot_layer = 4;
                        has_bot = true;
                    }
                }
            }

            if (has_top && has_bot) break;

            // Check backgrounds in priority order
            for (int bg = 0; bg < 4; bg++) {
                if (!(win_flags & (1 << bg))) continue;
                if (ppu->bg_scanline_valid[bg][x]) {
                    uint8_t p = (ppu->bg_scanline[bg][x] >> 20) & 0x3;
                    if (p == prio) {
                        if (!has_top) {
                            top_pixel = ppu->bg_scanline[bg][x];
                            top_color = top_pixel & 0x7FFF;
                            top_layer = bg;
                            has_top = true;
                        } else if (!has_bot) {
                            bot_pixel = ppu->bg_scanline[bg][x];
                            bot_color = bot_pixel & 0x7FFF;
                            bot_layer = bg;
                            has_bot = true;
                            break;
                        }
                    }
                }
            }

            if (has_top && has_bot) break;
        }

        uint16_t final_color = top_color;
        bool allow_effects = (win_flags & (1 << 5)) != 0;

        if (allow_effects) {
            if (semi_transparent_obj) {
                // Semi-transparent sprite always does alpha blending against lower layer
                if ((ppu->bldcnt & (1 << (8 + bot_layer))) || bot_layer == 5) {
                    final_color = blend_colors(top_color, bot_color, eva, evb);
                }
            } else if (ppu->bldcnt & (1 << top_layer)) {
                if (blend_mode == 1 && (ppu->bldcnt & (1 << (8 + bot_layer)))) {
                    // Alpha blending
                    final_color = blend_colors(top_color, bot_color, eva, evb);
                } else if (blend_mode == 2) {
                    // Brighten
                    final_color = brighten_color(top_color, evy);
                } else if (blend_mode == 3) {
                    // Darken
                    final_color = darken_color(top_color, evy);
                }
            }
        }

        ppu->gba->framebuffer[line * 240 + x] = rgb555_to_rgba32(final_color);
    }
}

void ppu_step(PPU* ppu, int cycles) {
    ppu->gba->scanline_cycles += cycles;

    // Scanline HDraw / HBlank cycle handling
    if (ppu->gba->scanline_cycles >= 960 && !(ppu->dispstat & 2)) {
        // Entering HBlank
        ppu->dispstat |= 2;
        if (ppu->vcount < 160) {
            ppu_render_scanline(ppu, ppu->vcount);
            dma_check_trigger(&ppu->gba->dma, DMA_START_HBLANK);
        }
        if (ppu->dispstat & (1 << 4)) {
            interrupt_request(&ppu->gba->interrupt, IRQ_HBLANK);
        }
    }

    if (ppu->gba->scanline_cycles >= CYCLES_PER_SCANLINE) {
        ppu->gba->scanline_cycles -= CYCLES_PER_SCANLINE;
        ppu->dispstat &= ~2; // End of HBlank

        // Advance scanline
        ppu->vcount++;
        if (ppu->vcount >= SCANLINES_PER_FRAME) {
            ppu->vcount = 0;
            ppu->dispstat &= ~1; // VBlank ends

            // Latch affine coordinates at start of frame
            ppu->bgx_internal[0] = ppu->bgx[0];
            ppu->bgy_internal[0] = ppu->bgy[0];
            ppu->bgx_internal[1] = ppu->bgx[1];
            ppu->bgy_internal[1] = ppu->bgy[1];

            ppu->gba->frame_ready = true;
        }

        // Increment internal affine Y coordinates on visible scanlines
        if (ppu->vcount < 160) {
            ppu->bgx_internal[0] += ppu->bgpb[0];
            ppu->bgy_internal[0] += ppu->bgpd[0];
            ppu->bgx_internal[1] += ppu->bgpb[1];
            ppu->bgy_internal[1] += ppu->bgpd[1];
        }

        // Check VCount Match
        uint8_t vcount_target = (ppu->dispstat >> 8) & 0xFF;
        if (ppu->vcount == vcount_target) {
            ppu->dispstat |= (1 << 2);
            if (ppu->dispstat & (1 << 5)) {
                interrupt_request(&ppu->gba->interrupt, IRQ_VCOUNT);
            }
        } else {
            ppu->dispstat &= ~(1 << 2);
        }

        // Check VBlank start
        if (ppu->vcount == 160) {
            ppu->dispstat |= 1; // Enter VBlank
            if (ppu->dispstat & (1 << 3)) {
                interrupt_request(&ppu->gba->interrupt, IRQ_VBLANK);
            }
            dma_check_trigger(&ppu->gba->dma, DMA_START_VBLANK);
        }
    }
}
