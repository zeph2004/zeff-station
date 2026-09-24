#include "bus.h"
#include "gba.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void bus_init(Bus* bus, GBA* gba) {
    memset(bus, 0, sizeof(Bus));
    bus->gba = gba;
    memset(bus->eeprom, 0xFF, sizeof(bus->eeprom));
}

void bus_destroy(Bus* bus) {
    if (bus->rom) {
        free(bus->rom);
        bus->rom = NULL;
    }
}

void bus_reset(Bus* bus) {
    memset(bus->ewram, 0, sizeof(bus->ewram));
    memset(bus->iwram, 0, sizeof(bus->iwram));
    memset(bus->io, 0, sizeof(bus->io));
    memset(bus->pram, 0, sizeof(bus->pram));
    memset(bus->vram, 0, sizeof(bus->vram));
    memset(bus->oam, 0, sizeof(bus->oam));
    bus->sram_dirty = false;

    if (!bus->bios_loaded) {
        static const uint32_t default_irq_trampoline[] = {
            0xE92D500F, // 0x18: stmdb sp!, {r0-r3, r12, lr}
            0xE3A00404, // 0x1C: mov r0, #0x04000000
            0xE5101004, // 0x20: ldr r1, [r0, #-4] (loads 0x03007FFC)
            0xE3510000, // 0x24: cmp r1, #0
            0x0A000001, // 0x28: beq +4 (to 0x34)
            0xE28FE000, // 0x2C: add lr, pc, #0 (LR = 0x34)
            0xE12FFF11, // 0x30: bx r1
            0xE8BD500F, // 0x34: ldmia sp!, {r0-r3, r12, lr}
            0xE25EF004, // 0x38: subs pc, lr, #4
        };
        memcpy(&bus->bios[0x18], default_irq_trampoline, sizeof(default_irq_trampoline));
    }
}

uint8_t bus_read8(Bus* bus, uint32_t addr) {
    uint8_t region = (addr >> 24) & 0xFF;

    switch (region) {
        case 0x00: // BIOS
            if (addr < 0x4000) {
                return bus->bios[addr];
            }
            return 0;

        case 0x02: // EWRAM (256 KB)
            return bus->ewram[addr & 0x3FFFF];

        case 0x03: // IWRAM (32 KB)
            return bus->iwram[addr & 0x7FFF];

        case 0x04: { // I/O Registers
            uint32_t io_addr = addr & 0x3FF;
            if (io_addr == REG_KEYINPUT || io_addr == REG_KEYINPUT + 1) {
                uint16_t kp = bus->gba->keypad.keyinput;
                return (io_addr & 1) ? (uint8_t)(kp >> 8) : (uint8_t)(kp & 0xFF);
            }
            if (io_addr >= REG_TM0CNT_L && io_addr <= REG_TM3CNT_H + 1) {
                int tm_idx = (io_addr - REG_TM0CNT_L) / 4;
                int sub = (io_addr - REG_TM0CNT_L) % 4;
                if (sub == 0 || sub == 1) {
                    uint16_t cnt = timer_read_counter(&bus->gba->timers, tm_idx);
                    return (sub == 1) ? (uint8_t)(cnt >> 8) : (uint8_t)(cnt & 0xFF);
                }
            }
            if (io_addr >= REG_DISPCNT && io_addr <= REG_BLDY + 1) {
                uint16_t val = ppu_read16(&bus->gba->ppu, io_addr & ~1U);
                return (io_addr & 1) ? (uint8_t)(val >> 8) : (uint8_t)(val & 0xFF);
            }
            if (io_addr == REG_IE || io_addr == REG_IE + 1) {
                return (io_addr & 1) ? (uint8_t)(bus->gba->interrupt.ie >> 8) : (uint8_t)(bus->gba->interrupt.ie & 0xFF);
            }
            if (io_addr == REG_IF || io_addr == REG_IF + 1) {
                return (io_addr & 1) ? (uint8_t)(bus->gba->interrupt.if_ >> 8) : (uint8_t)(bus->gba->interrupt.if_ & 0xFF);
            }
            if (io_addr == REG_IME || io_addr == REG_IME + 1) {
                return (io_addr & 1) ? 0 : (uint8_t)(bus->gba->interrupt.ime & 1);
            }
            if (io_addr >= REG_SOUND1CNT_L && io_addr <= 0x00A8) {
                return apu_read8(&bus->gba->apu, io_addr);
            }
            return bus->io[io_addr];
        }

        case 0x05: // Palette RAM
            return bus->pram[addr & 0x3FF];

        case 0x06: { // VRAM
            uint32_t vaddr = addr & 0x1FFFF;
            if (vaddr >= 0x18000) vaddr &= 0x17FFF;
            return bus->vram[vaddr];
        }

        case 0x07: // OAM
            return bus->oam[addr & 0x3FF];

        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C: { // ROM
            if (bus->rom && bus->rom_size > 0) {
                uint32_t offset = (addr & 0x01FFFFFF) % bus->rom_size;
                return bus->rom[offset];
            }
            return 0;
        }

        case 0x0D:
            return 1; // EEPROM ready

        case 0x0E:
        case 0x0F: // SRAM
            return bus->sram[addr & 0xFFFF];

        default:
            return 1;
    }
}

uint16_t bus_read16(Bus* bus, uint32_t addr) {
    addr &= ~1U;
    uint8_t region = (addr >> 24) & 0xFF;

    switch (region) {
        case 0x00:
            if (addr < 0x4000) {
                return *(uint16_t*)&bus->bios[addr];
            }
            return 0;

        case 0x02:
            return *(uint16_t*)&bus->ewram[addr & 0x3FFFF];

        case 0x03:
            return *(uint16_t*)&bus->iwram[addr & 0x7FFF];

        case 0x04: {
            uint32_t io_addr = addr & 0x3FF;
            if (io_addr == REG_KEYINPUT) {
                return bus->gba->keypad.keyinput;
            }
            if (io_addr >= REG_TM0CNT_L && io_addr <= REG_TM3CNT_H) {
                int tm_idx = (io_addr - REG_TM0CNT_L) / 4;
                int sub = (io_addr - REG_TM0CNT_L) % 4;
                if (sub == 0) {
                    return timer_read_counter(&bus->gba->timers, tm_idx);
                }
            }
            if (io_addr >= REG_DISPCNT && io_addr <= REG_BLDY) {
                return ppu_read16(&bus->gba->ppu, io_addr);
            }
            if (io_addr == REG_IE) return bus->gba->interrupt.ie;
            if (io_addr == REG_IF) return bus->gba->interrupt.if_;
            if (io_addr == REG_IME) return bus->gba->interrupt.ime;
            if (io_addr >= REG_SOUND1CNT_L && io_addr <= 0x00A8) {
                return apu_read16(&bus->gba->apu, io_addr);
            }
            return *(uint16_t*)&bus->io[io_addr];
        }

        case 0x05:
            return *(uint16_t*)&bus->pram[addr & 0x3FF];

        case 0x06: {
            uint32_t vaddr = addr & 0x1FFFF;
            if (vaddr >= 0x18000) vaddr &= 0x17FFF;
            return *(uint16_t*)&bus->vram[vaddr];
        }

        case 0x07:
            return *(uint16_t*)&bus->oam[addr & 0x3FF];

        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C: {
            if (bus->rom && bus->rom_size > 0) {
                uint32_t offset = (addr & 0x01FFFFFF) % bus->rom_size;
                if ((offset + 1) < bus->rom_size) {
                    return *(uint16_t*)&bus->rom[offset];
                }
                return bus->rom[offset];
            }
            return 0;
        }

        case 0x0D:
            return 1; // EEPROM ready

        case 0x0E:
        case 0x0F: {
            uint8_t byte = bus->sram[addr & 0xFFFF];
            return (uint16_t)byte | ((uint16_t)byte << 8);
        }

        default:
            return 1;
    }
}

uint32_t bus_read32(Bus* bus, uint32_t addr) {
    addr &= ~3U;
    uint8_t region = (addr >> 24) & 0xFF;

    switch (region) {
        case 0x00:
            if (addr < 0x4000) {
                return *(uint32_t*)&bus->bios[addr];
            }
            return 0;

        case 0x02:
            return *(uint32_t*)&bus->ewram[addr & 0x3FFFF];

        case 0x03:
            return *(uint32_t*)&bus->iwram[addr & 0x7FFF];

        case 0x04: {
            uint16_t lo = bus_read16(bus, addr);
            uint16_t hi = bus_read16(bus, addr + 2);
            return (uint32_t)lo | ((uint32_t)hi << 16);
        }

        case 0x05:
            return *(uint32_t*)&bus->pram[addr & 0x3FF];

        case 0x06: {
            uint32_t vaddr = addr & 0x1FFFF;
            if (vaddr >= 0x18000) vaddr &= 0x17FFF;
            return *(uint32_t*)&bus->vram[vaddr];
        }

        case 0x07:
            return *(uint32_t*)&bus->oam[addr & 0x3FF];

        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C: {
            if (bus->rom && bus->rom_size > 0) {
                uint32_t offset = (addr & 0x01FFFFFF) % bus->rom_size;
                if ((offset + 3) < bus->rom_size) {
                    return *(uint32_t*)&bus->rom[offset];
                }
                uint16_t lo = bus_read16(bus, addr);
                uint16_t hi = bus_read16(bus, addr + 2);
                return (uint32_t)lo | ((uint32_t)hi << 16);
            }
            return 0;
        }

        case 0x0D:
            return 1; // EEPROM ready

        case 0x0E:
        case 0x0F: {
            uint8_t byte = bus->sram[addr & 0xFFFF];
            return (uint32_t)byte | ((uint32_t)byte << 8) | ((uint32_t)byte << 16) | ((uint32_t)byte << 24);
        }

        default:
            return 1;
    }
}

void bus_write8(Bus* bus, uint32_t addr, uint8_t val) {
    uint8_t region = (addr >> 24) & 0xFF;

    switch (region) {
        case 0x02:
            bus->ewram[addr & 0x3FFFF] = val;
            break;

        case 0x03:
            bus->iwram[addr & 0x7FFF] = val;
            break;

        case 0x04: {
            uint32_t io_addr = addr & 0x3FF;
            if (io_addr >= REG_SOUND1CNT_L && io_addr <= 0x00A8) {
                apu_write8(&bus->gba->apu, io_addr, val);
                return;
            }
            if (io_addr == REG_HALTCNT) {
                if (val & 0x80) {
                    bus->gba->stopped = true;
                } else {
                    bus->gba->halted = true;
                }
                return;
            }
            // For general 16-bit registers written as 8-bit:
            uint16_t current = bus_read16(bus, io_addr & ~1U);
            if (io_addr & 1) {
                bus_write16(bus, io_addr & ~1U, (current & 0x00FF) | ((uint16_t)val << 8));
            } else {
                bus_write16(bus, io_addr & ~1U, (current & 0xFF00) | val);
            }
            break;
        }

        case 0x05: {
            // 8-bit write to palette writes byte to both bytes of halfword
            uint32_t paddr = addr & ~1U & 0x3FF;
            bus->pram[paddr] = val;
            bus->pram[paddr + 1] = val;
            break;
        }

        case 0x06: {
            // 8-bit write to VRAM: writes to both bytes of halfword in BG VRAM, ignored in OBJ VRAM
            uint32_t vaddr = addr & 0x1FFFF;
            if (vaddr >= 0x18000) vaddr &= 0x17FFF;
            if (vaddr < 0x10000) {
                vaddr &= ~1U;
                bus->vram[vaddr] = val;
                bus->vram[vaddr + 1] = val;
            }
            break;
        }

        case 0x07:
            // 8-bit write to OAM is ignored
            break;

        case 0x0E:
        case 0x0F:
            bus->sram[addr & 0xFFFF] = val;
            bus->sram_dirty = true;
            bus->has_sram = true;
            break;

        default:
            break;
    }
}

void bus_write16(Bus* bus, uint32_t addr, uint16_t val) {
    addr &= ~1U;
    uint8_t region = (addr >> 24) & 0xFF;

    switch (region) {
        case 0x02:
            *(uint16_t*)&bus->ewram[addr & 0x3FFFF] = val;
            break;

        case 0x03:
            *(uint16_t*)&bus->iwram[addr & 0x7FFF] = val;
            break;

        case 0x04: {
            uint32_t io_addr = addr & 0x3FF;
            bus->io[io_addr] = (uint8_t)(val & 0xFF);
            bus->io[io_addr + 1] = (uint8_t)(val >> 8);

            if (io_addr >= REG_DISPCNT && io_addr <= REG_BLDY) {
                ppu_write16(&bus->gba->ppu, io_addr, val);
                return;
            }
            if (io_addr >= REG_SOUND1CNT_L && io_addr <= 0x00A8) {
                apu_write16(&bus->gba->apu, io_addr, val);
                return;
            }
            if (io_addr >= REG_DMA0SAD_L && io_addr <= REG_DMA3CNT_H) {
                int ch = (io_addr - REG_DMA0SAD_L) / 12;
                int reg_offset = (io_addr - REG_DMA0SAD_L) % 12;
                switch (reg_offset) {
                    case 0: bus->gba->dma.channels[ch].sad = (bus->gba->dma.channels[ch].sad & 0xFFFF0000) | val; break;
                    case 2: bus->gba->dma.channels[ch].sad = (bus->gba->dma.channels[ch].sad & 0x0000FFFF) | ((uint32_t)val << 16); break;
                    case 4: bus->gba->dma.channels[ch].dad = (bus->gba->dma.channels[ch].dad & 0xFFFF0000) | val; break;
                    case 6: bus->gba->dma.channels[ch].dad = (bus->gba->dma.channels[ch].dad & 0x0000FFFF) | ((uint32_t)val << 16); break;
                    case 8: bus->gba->dma.channels[ch].word_count = val; break;
                    case 10: dma_write_cnt_h(&bus->gba->dma, ch, val); break;
                }
                return;
            }
            if (io_addr >= REG_TM0CNT_L && io_addr <= REG_TM3CNT_H) {
                int ch = (io_addr - REG_TM0CNT_L) / 4;
                int reg_offset = (io_addr - REG_TM0CNT_L) % 4;
                if (reg_offset == 0) {
                    timer_write_reload(&bus->gba->timers, ch, val);
                } else {
                    timer_write_control(&bus->gba->timers, ch, val);
                }
                return;
            }
            if (io_addr == REG_KEYCNT) {
                bus->gba->keypad.keycnt = val;
                keypad_check_irq(&bus->gba->keypad);
                return;
            }
            if (io_addr == REG_IE) {
                bus->gba->interrupt.ie = val & 0x3FFF;
                return;
            }
            if (io_addr == REG_IF) {
                // Writing 1 clears the flag!
                bus->gba->interrupt.if_ &= ~val;
                return;
            }
            if (io_addr == REG_IME) {
                bus->gba->interrupt.ime = val & 1;
                return;
            }
            if (io_addr == REG_HALTCNT) {
                if (val & 0x80) bus->gba->stopped = true;
                else bus->gba->halted = true;
                return;
            }
            break;
        }

        case 0x05:
            *(uint16_t*)&bus->pram[addr & 0x3FF] = val;
            break;

        case 0x06: {
            uint32_t vaddr = addr & 0x1FFFF;
            if (vaddr >= 0x18000) vaddr &= 0x17FFF;
            *(uint16_t*)&bus->vram[vaddr] = val;
            break;
        }

        case 0x07:
            *(uint16_t*)&bus->oam[addr & 0x3FF] = val;
            break;

        case 0x0E:
        case 0x0F:
            bus->sram[addr & 0xFFFF] = (uint8_t)(val & 0xFF);
            bus->sram_dirty = true;
            bus->has_sram = true;
            break;

        default:
            break;
    }
}

void bus_write32(Bus* bus, uint32_t addr, uint32_t val) {
    addr &= ~3U;
    uint8_t region = (addr >> 24) & 0xFF;

    switch (region) {
        case 0x02:
            *(uint32_t*)&bus->ewram[addr & 0x3FFFF] = val;
            break;

        case 0x03:
            *(uint32_t*)&bus->iwram[addr & 0x7FFF] = val;
            break;

        case 0x04:
            bus_write16(bus, addr, (uint16_t)(val & 0xFFFF));
            bus_write16(bus, addr + 2, (uint16_t)(val >> 16));
            break;

        case 0x05:
            *(uint32_t*)&bus->pram[addr & 0x3FF] = val;
            break;

        case 0x06: {
            uint32_t vaddr = addr & 0x1FFFF;
            if (vaddr >= 0x18000) vaddr &= 0x17FFF;
            *(uint32_t*)&bus->vram[vaddr] = val;
            break;
        }

        case 0x07:
            *(uint32_t*)&bus->oam[addr & 0x3FF] = val;
            break;

        case 0x0E:
        case 0x0F:
            bus->sram[addr & 0xFFFF] = (uint8_t)(val & 0xFF);
            bus->sram_dirty = true;
            bus->has_sram = true;
            break;

        default:
            break;
    }
}

uint8_t* bus_get_ptr(Bus* bus, uint32_t addr) {
    uint8_t region = (addr >> 24) & 0xFF;
    switch (region) {
        case 0x00: return (addr < 0x4000) ? &bus->bios[addr] : NULL;
        case 0x02: return &bus->ewram[addr & 0x3FFFF];
        case 0x03: return &bus->iwram[addr & 0x7FFF];
        case 0x04: return &bus->io[addr & 0x3FF];
        case 0x05: return &bus->pram[addr & 0x3FF];
        case 0x06: {
            uint32_t vaddr = addr & 0x1FFFF;
            if (vaddr >= 0x18000) vaddr &= 0x17FFF;
            return &bus->vram[vaddr];
        }
        case 0x07: return &bus->oam[addr & 0x3FF];
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C: {
            if (bus->rom && bus->rom_size > 0) {
                uint32_t offset = (addr & 0x01FFFFFF) % bus->rom_size;
                return &bus->rom[offset];
            }
            return NULL;
        }
        case 0x0E:
        case 0x0F: return &bus->sram[addr & 0xFFFF];
        default: return NULL;
    }
}

void bus_save_sram(Bus* bus, const char* filepath) {
    if (!bus->sram_dirty || !filepath || filepath[0] == '\0') return;
    FILE* f = fopen(filepath, "wb");
    if (f) {
        if (bus->has_eeprom && !bus->has_sram) {
            fwrite(bus->eeprom, 1, sizeof(bus->eeprom), f);
        } else {
            fwrite(bus->sram, 1, sizeof(bus->sram), f);
        }
        fclose(f);
        bus->sram_dirty = false;
    }
}

void bus_load_sram(Bus* bus, const char* filepath) {
    if (!filepath || filepath[0] == '\0') return;
    FILE* f = fopen(filepath, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 0 && sz <= (long)sizeof(bus->eeprom)) {
            fread(bus->eeprom, 1, sz, f);
            bus->has_eeprom = true;
        } else if (sz > 0) {
            size_t to_read = (sz < (long)sizeof(bus->sram)) ? (size_t)sz : sizeof(bus->sram);
            fread(bus->sram, 1, to_read, f);
            bus->has_sram = true;
        }
        fclose(f);
        bus->sram_dirty = false;
    }
}
