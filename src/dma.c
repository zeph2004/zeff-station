#include "dma.h"
#include "gba.h"
#include "bus.h"
#include <string.h>
#include <stdio.h>

void dma_init(DMAController* dma, GBA* gba) {
    memset(dma, 0, sizeof(DMAController));
    dma->gba = gba;
}

void dma_reset(DMAController* dma) {
    memset(dma->channels, 0, sizeof(dma->channels));
}

void dma_write_cnt_h(DMAController* dma, int ch, uint16_t val) {
    DmaChannel* c = &dma->channels[ch];
    bool was_enabled = c->enabled;
    c->control = val;

    c->dest_adj = (val >> 5) & 0x3;
    c->src_adj = (val >> 7) & 0x3;
    c->repeat = (val & (1 << 9)) != 0;
    c->is_32bit = (val & (1 << 10)) != 0;
    c->start_timing = (DmaStartTiming)((val >> 12) & 0x3);
    c->irq_on_finish = (val & (1 << 14)) != 0;
    c->enabled = (val & (1 << 15)) != 0;

    if (!was_enabled && c->enabled) {
        // Latch internal addresses and counts on 0 -> 1 transition
        c->internal_sad = c->sad;
        c->internal_dad = c->dad;
        uint32_t max_count = (ch == 3) ? 0x10000 : 0x4000;
        c->internal_count = (c->word_count == 0) ? max_count : c->word_count;

        if (c->start_timing == DMA_START_IMMEDIATE) {
            c->pending = true;
            dma_step(dma);
        }
    }
}

static void dma_transfer_channel(DMAController* dma, int ch) {
    DmaChannel* c = &dma->channels[ch];
    Bus* bus = &dma->gba->bus;

    int step_size = c->is_32bit ? 4 : 2;
    int src_step = (c->src_adj == 0) ? step_size : ((c->src_adj == 1) ? -step_size : 0);
    int dst_step = (c->dest_adj == 0 || c->dest_adj == 3) ? step_size : ((c->dest_adj == 1) ? -step_size : 0);

    uint32_t count = c->internal_count;
    uint8_t src_region = (c->internal_sad >> 24) & 0xFF;
    uint8_t dst_region = (c->internal_dad >> 24) & 0xFF;

    // Handle EEPROM transfers (typically DMA3 to/from 0x0D000000)
    if (dst_region == 0x0D) {
        bus->has_eeprom = true;
        uint8_t bits[128];
        uint32_t nbits = (count > 128) ? 128 : count;
        for (uint32_t i = 0; i < nbits; i++) {
            bits[i] = (uint8_t)(bus_read16(bus, c->internal_sad + i * 2) & 1);
        }

        uint8_t cmd = (bits[0] << 1) | bits[1];
        if (cmd == 3) {
            // Read Request: 11
            if (count <= 9) {
                // 4Kbit EEPROM (6 address bits)
                uint16_t addr = 0;
                for (int i = 2; i < 8; i++) addr = (addr << 1) | bits[i];
                bus->eeprom_read_addr = addr & 0x3F;
            } else {
                // 64Kbit EEPROM (14 address bits)
                uint16_t addr = 0;
                for (int i = 2; i < 16; i++) addr = (addr << 1) | bits[i];
                bus->eeprom_read_addr = addr & 0x3FF;
            }
        } else if (cmd == 2) {
            // Write Request: 10
            if (count <= 73) {
                // 4Kbit EEPROM (6 address bits + 64 data bits)
                uint16_t addr = 0;
                for (int i = 2; i < 8; i++) addr = (addr << 1) | bits[i];
                addr &= 0x3F;
                for (int b = 0; b < 64; b++) {
                    int byte_idx = (addr * 8) + (b / 8);
                    int bit_pos = 7 - (b % 8);
                    if (b % 8 == 0) bus->eeprom[byte_idx] = 0;
                    bus->eeprom[byte_idx] |= (bits[8 + b] << bit_pos);
                }
            } else {
                // 64Kbit EEPROM (14 address bits + 64 data bits)
                uint16_t addr = 0;
                for (int i = 2; i < 16; i++) addr = (addr << 1) | bits[i];
                addr &= 0x3FF;
                for (int b = 0; b < 64; b++) {
                    int byte_idx = (addr * 8) + (b / 8);
                    int bit_pos = 7 - (b % 8);
                    if (b % 8 == 0) bus->eeprom[byte_idx] = 0;
                    bus->eeprom[byte_idx] |= (bits[16 + b] << bit_pos);
                }
            }
            bus->sram_dirty = true;
        }

        c->internal_sad += src_step * count;
        c->internal_dad += dst_step * count;

        if (c->irq_on_finish) {
            interrupt_request(&dma->gba->interrupt, (uint16_t)(IRQ_DMA0 << ch));
        }
        c->enabled = false;
        c->control &= ~(1 << 15);
        uint32_t io_reg = REG_DMA0CNT_H + ch * 12;
        bus->io[io_reg + 1] &= 0x7F;
        c->pending = false;
        return;
    } else if (src_region == 0x0D) {
        // EEPROM Read response
        bus->has_eeprom = true;
        uint16_t out_words[68];
        memset(out_words, 0, sizeof(out_words));
        uint16_t addr = bus->eeprom_read_addr & 0x3FF;
        for (int b = 0; b < 64; b++) {
            int byte_idx = (addr * 8) + (b / 8);
            int bit_pos = 7 - (b % 8);
            out_words[4 + b] = (bus->eeprom[byte_idx] >> bit_pos) & 1;
        }

        for (uint32_t i = 0; i < count; i++) {
            uint16_t val = (i < 68) ? out_words[i] : 1;
            bus_write16(bus, c->internal_dad, val);
            c->internal_dad += dst_step;
        }
        c->internal_sad += src_step * count;

        if (c->irq_on_finish) {
            interrupt_request(&dma->gba->interrupt, (uint16_t)(IRQ_DMA0 << ch));
        }
        c->enabled = false;
        c->control &= ~(1 << 15);
        uint32_t io_reg = REG_DMA0CNT_H + ch * 12;
        bus->io[io_reg + 1] &= 0x7F;
        c->pending = false;
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (c->is_32bit) {
            uint32_t val = bus_read32(bus, c->internal_sad);
            bus_write32(bus, c->internal_dad, val);
        } else {
            uint16_t val = bus_read16(bus, c->internal_sad);
            bus_write16(bus, c->internal_dad, val);
        }
        c->internal_sad += src_step;
        c->internal_dad += dst_step;
    }

    if (c->irq_on_finish) {
        interrupt_request(&dma->gba->interrupt, (uint16_t)(IRQ_DMA0 << ch));
    }

    if (c->repeat) {
        if (c->dest_adj == 3) {
            c->internal_dad = c->dad; // Reload dest address
        }
        uint32_t max_count = (ch == 3) ? 0x10000 : 0x4000;
        c->internal_count = (c->word_count == 0) ? max_count : c->word_count;
    } else {
        c->enabled = false;
        c->control &= ~(1 << 15);
        // Reflect in I/O register space
        uint32_t io_reg = REG_DMA0CNT_H + ch * 12;
        bus->io[io_reg + 1] &= 0x7F;
    }
    c->pending = false;
}

void dma_trigger_sound_fifo(DMAController* dma, int fifo_ch) {
    // fifo_ch: 0 = FIFO A (DMA1), 1 = FIFO B (DMA2)
    int ch = (fifo_ch == 0) ? 1 : 2;
    DmaChannel* c = &dma->channels[ch];

    if (!c->enabled || c->start_timing != DMA_START_SPECIAL) {
        return;
    }

    Bus* bus = &dma->gba->bus;
    uint32_t dest = (fifo_ch == 0) ? 0x040000A0 : 0x040000A4;

    // FIFO DMA always transfers 4 words (16 bytes = 16 samples)
    for (int i = 0; i < 4; i++) {
        uint32_t val = bus_read32(bus, c->internal_sad);
        bus_write32(bus, dest, val);
        c->internal_sad += 4;
    }

    if (c->irq_on_finish) {
        interrupt_request(&dma->gba->interrupt, (uint16_t)(IRQ_DMA0 << ch));
    }
}

void dma_check_trigger(DMAController* dma, DmaStartTiming timing) {
    for (int ch = 0; ch < 4; ch++) {
        DmaChannel* c = &dma->channels[ch];
        if (c->enabled && c->start_timing == timing) {
            c->pending = true;
        }
    }
    dma_step(dma);
}

void dma_step(DMAController* dma) {
    // Priority: DMA0 > DMA1 > DMA2 > DMA3
    for (int ch = 0; ch < 4; ch++) {
        if (dma->channels[ch].pending) {
            dma_transfer_channel(dma, ch);
        }
    }
}
