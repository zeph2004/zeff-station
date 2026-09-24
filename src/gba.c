#include "gba.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

GBA* gba_create(void) {
    GBA* gba = (GBA*)calloc(1, sizeof(GBA));
    if (!gba) return NULL;

    bus_init(&gba->bus, gba);
    arm7tdmi_init(&gba->cpu, gba);
    ppu_init(&gba->ppu, gba);
    dma_init(&gba->dma, gba);
    timer_init(&gba->timers, gba);
    interrupt_init(&gba->interrupt, gba);
    apu_init(&gba->apu, gba);
    keypad_init(&gba->keypad, gba);

    return gba;
}

void gba_destroy(GBA* gba) {
    if (!gba) return;
    if (gba->save_filepath[0] != '\0') {
        bus_save_sram(&gba->bus, gba->save_filepath);
    }
    bus_destroy(&gba->bus);
    free(gba);
}

void gba_reset(GBA* gba, bool skip_intro) {
    bus_reset(&gba->bus);
    ppu_reset(&gba->ppu);
    dma_reset(&gba->dma);
    timer_reset(&gba->timers);
    interrupt_reset(&gba->interrupt);
    apu_reset(&gba->apu);
    keypad_reset(&gba->keypad);

    gba->total_cycles = 0;
    gba->frame_cycles = 0;
    gba->scanline_cycles = 0;
    gba->current_scanline = 0;
    gba->halted = false;
    gba->stopped = false;
    gba->frame_ready = false;
    gba->skip_intro = skip_intro;
    gba->intro_frame_counter = 0;

    memset(gba->framebuffer, 0, sizeof(gba->framebuffer));

    if (skip_intro) {
        bios_setup_post_boot(gba);
    } else {
        gba->intro_completed = false;
    }
}

bool gba_load_rom(GBA* gba, const char* rom_path) {
    FILE* f = fopen(rom_path, "rb");
    if (!f) {
        fprintf(stderr, "[Zeff Station] Failed to open ROM: %s\n", rom_path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > ROM_MAX_SIZE) {
        fprintf(stderr, "[Zeff Station] Invalid ROM size: %ld bytes\n", size);
        fclose(f);
        return false;
    }

    // Allocate ROM buffer padded to power of 2 or at least size
    gba->bus.rom = (uint8_t*)malloc(size);
    if (!gba->bus.rom) {
        fclose(f);
        return false;
    }

    if (fread(gba->bus.rom, 1, size, f) != (size_t)size) {
        fprintf(stderr, "[Zeff Station] Failed reading ROM file\n");
        fclose(f);
        return false;
    }
    fclose(f);

    gba->bus.rom_size = (size_t)size;

    // Read ROM Title from offset 0xA0 (up to 12 chars)
    if (gba->bus.rom_size >= 0xAC) {
        memcpy(gba->rom_title, &gba->bus.rom[0xA0], 12);
        gba->rom_title[12] = '\0';
        for (int i = 0; i < 12; i++) {
            if ((unsigned char)gba->rom_title[i] < 32 || (unsigned char)gba->rom_title[i] > 126) {
                gba->rom_title[i] = '\0';
                break;
            }
        }
    } else {
        strcpy(gba->rom_title, "UNKNOWN");
    }

    // Setup save file path (.sav alongside ROM)
    strncpy(gba->save_filepath, rom_path, sizeof(gba->save_filepath) - 5);
    char* dot = strrchr(gba->save_filepath, '.');
    if (dot) {
        strcpy(dot, ".sav");
    } else {
        strcat(gba->save_filepath, ".sav");
    }

    bus_load_sram(&gba->bus, gba->save_filepath);

    printf("=====================================================\n");
    printf("   ZEFF STATION GBA EMULATION SYSTEM                \n");
    printf("=====================================================\n");
    printf(" Loaded ROM:    %s\n", rom_path);
    printf(" Game Title:    %s\n", gba->rom_title);
    printf(" ROM Size:      %.2f MB (%ld bytes)\n", (double)size / (1024.0 * 1024.0), size);
    printf(" Save Path:     %s\n", gba->save_filepath);
    printf("=====================================================\n");

    return true;
}

bool gba_load_bios(GBA* gba, const char* bios_path) {
    FILE* f = fopen(bios_path, "rb");
    if (!f) return false;

    size_t read = fread(gba->bus.bios, 1, BIOS_SIZE, f);
    fclose(f);

    if (read == BIOS_SIZE) {
        gba->bus.bios_loaded = true;
        printf("[Zeff Station] External BIOS loaded successfully (%zu bytes)\n", read);
        return true;
    }
    return false;
}

void gba_step(GBA* gba) {
    int cycles = arm7tdmi_step(&gba->cpu);
    if (cycles <= 0) cycles = 1;

    gba->total_cycles += cycles;
    gba->frame_cycles += cycles;

    timer_step(&gba->timers, cycles);
    apu_step(&gba->apu, cycles);
    ppu_step(&gba->ppu, cycles);
}

void gba_run_frame(GBA* gba) {
    if (!gba->intro_completed) {
        bios_render_intro_frame(gba, gba->intro_frame_counter++);
        // Advance APU for jingle
        apu_step(&gba->apu, CYCLES_PER_FRAME);
        if (gba->intro_frame_counter >= 90) {
            bios_setup_post_boot(gba);
        }
        gba->frame_ready = true;
        return;
    }

    gba->frame_ready = false;
    while (!gba->frame_ready) {
        gba_step(gba);
    }
}
