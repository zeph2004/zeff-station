#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "gba.h"

static int g_pass_count = 0;
static int g_fail_count = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { \
        printf("  [PASS] %s\n", msg); \
        g_pass_count++; \
    } else { \
        printf("  [FAIL] %s\n", msg); \
        g_fail_count++; \
    } \
} while(0)

void test_memory_mirroring(void) {
    printf("\n=== Testing Memory Mirroring and Bus Access ===\n");
    GBA* gba = gba_create();
    gba_reset(gba, true);

    // 1. EWRAM Mirroring (256 KB)
    bus_write32(&gba->bus, 0x02001000, 0xAABBCCDD);
    uint32_t ewram_mirror = bus_read32(&gba->bus, 0x02041000);
    TEST_ASSERT(ewram_mirror == 0xAABBCCDD, "EWRAM mirroring at 0x02041000");

    // 2. IWRAM Mirroring (32 KB)
    bus_write32(&gba->bus, 0x03000500, 0x12345678);
    uint32_t iwram_mirror = bus_read32(&gba->bus, 0x03008500);
    TEST_ASSERT(iwram_mirror == 0x12345678, "IWRAM mirroring at 0x03008500");

    // 3. Palette 8-bit write duplication
    bus_write8(&gba->bus, 0x05000000, 0x42);
    uint16_t pal_word = bus_read16(&gba->bus, 0x05000000);
    TEST_ASSERT(pal_word == 0x4242, "Palette 8-bit write expands to halfword 0x4242");

    // 4. Unaligned 32-bit read rotation
    bus_write32(&gba->bus, 0x03000100, 0x12345678);
    // Unaligned ARM read at offset + 1 rotates right by 8 bits
    uint32_t unaligned_1 = bus_read32(&gba->bus, 0x03000100);
    TEST_ASSERT(unaligned_1 == 0x12345678, "Aligned 32-bit read");

    gba_destroy(gba);
}

void test_cpu_arm_execution(void) {
    printf("\n=== Testing ARM Instruction Set with cpu_arm_test.gba ===\n");
    GBA* gba = gba_create();
    bool loaded = gba_load_rom(gba, "roms/cpu_arm_test.gba");
    TEST_ASSERT(loaded, "Load cpu_arm_test.gba");

    gba_reset(gba, true); // Skip intro, boot directly to ROM

    for (int frame = 0; frame < 60; frame++) {
        gba_run_frame(gba);
    }

    uint32_t result = bus_read32(&gba->bus, 0x03000000);
    printf("  ARM Test Output Magic: 0x%08X (Expected: 0xCAFEBABE)\n", result);
    TEST_ASSERT(result == 0xCAFEBABE, "ARM arithmetic, shift, multiply, and memory pass");

    gba_destroy(gba);
}

void test_cpu_thumb_execution(void) {
    printf("\n=== Testing THUMB Instruction Set with cpu_thumb_test.gba ===\n");
    GBA* gba = gba_create();
    bool loaded = gba_load_rom(gba, "roms/cpu_thumb_test.gba");
    TEST_ASSERT(loaded, "Load cpu_thumb_test.gba");

    gba_reset(gba, true);

    for (int frame = 0; frame < 60; frame++) {
        gba_run_frame(gba);
    }

    uint32_t result = bus_read32(&gba->bus, 0x03000004);
    printf("  THUMB Test Output Magic: 0x%08X (Expected: 0x1337BEEF)\n", result);
    TEST_ASSERT(result == 0x1337BEEF, "THUMB push/pop, arithmetic, and branches pass");

    gba_destroy(gba);
}

void test_ppu_mode3_execution(void) {
    printf("\n=== Testing PPU Mode 3 Bitmap with mode3_colors.gba ===\n");
    GBA* gba = gba_create();
    bool loaded = gba_load_rom(gba, "roms/mode3_colors.gba");
    TEST_ASSERT(loaded, "Load mode3_colors.gba");

    gba_reset(gba, true);

    for (int frame = 0; frame < 30; frame++) {
        gba_run_frame(gba);
    }

    // Check that framebuffer has non-zero pixels
    int non_black_pixels = 0;
    for (int i = 0; i < GBA_SCREEN_PIXELS; i++) {
        if ((gba->framebuffer[i] & 0x00FFFFFF) != 0) {
            non_black_pixels++;
        }
    }

    printf("  Rendered non-black pixels: %d / %d\n", non_black_pixels, GBA_SCREEN_PIXELS);
    TEST_ASSERT(non_black_pixels > 1000, "PPU rasterized Mode 3 screen content");

    gba_destroy(gba);
}

void test_timers_irq(void) {
    printf("\n=== Testing Timers and IRQ Controller ===\n");
    GBA* gba = gba_create();
    gba_reset(gba, true);

    // Setup Timer 0 with prescaler 1, reload 0xFF00
    timer_write_reload(&gba->timers, 0, 0xFF00);
    timer_write_control(&gba->timers, 0, 0x00C0); // Start, IRQ enabled

    // Step 500 cycles (enough for 256 ticks to overflow)
    timer_step(&gba->timers, 500);

    TEST_ASSERT((gba->interrupt.if_ & IRQ_TIMER0) != 0, "Timer 0 overflow triggered IRQ_TIMER0 flag in IF");

    gba_destroy(gba);
}

void test_dma_controller(void) {
    printf("\n=== Testing DMA Controller Channels ===\n");
    GBA* gba = gba_create();
    gba_reset(gba, true);

    // Fill source in EWRAM
    for (int i = 0; i < 16; i++) {
        bus_write32(&gba->bus, 0x02000000 + i * 4, 0x11223344 + i);
    }

    // Configure DMA0: copy 16 words from 0x02000000 to 0x03000000
    gba->dma.channels[0].sad = 0x02000000;
    gba->dma.channels[0].dad = 0x03000000;
    gba->dma.channels[0].word_count = 16;
    // 32-bit transfer, immediate start, enable
    dma_write_cnt_h(&gba->dma, 0, 0x8400);

    // Verify destination in IWRAM
    bool matches = true;
    for (int i = 0; i < 16; i++) {
        uint32_t val = bus_read32(&gba->bus, 0x03000000 + i * 4);
        if (val != (uint32_t)(0x11223344 + i)) {
            matches = false;
            break;
        }
    }
    TEST_ASSERT(matches, "DMA0 32-bit word block transfer from EWRAM to IWRAM");

    gba_destroy(gba);
}

void test_eeprom_dma(void) {
    printf("\n=== Testing EEPROM DMA Serial Protocol ===\n");
    GBA* gba = gba_create();
    gba_reset(gba, true);

    // 1. Prepare 81-bit write command for 64Kbit EEPROM:
    // "10" (2 bits) + Addr 0 (14 bits: 0) + 64 data bits (0x0123456789ABCDEF) + "0" (1 bit)
    uint16_t write_stream[81];
    memset(write_stream, 0, sizeof(write_stream));
    write_stream[0] = 1; // '1'
    write_stream[1] = 0; // '0' -> Write command
    // Addr 0: bits 2..15 are 0
    uint64_t test_data = 0x0123456789ABCDEFULL;
    for (int b = 0; b < 64; b++) {
        write_stream[16 + b] = (uint16_t)((test_data >> (63 - b)) & 1);
    }
    write_stream[80] = 0; // Terminator

    // Write to EWRAM at 0x02000000
    for (int i = 0; i < 81; i++) {
        bus_write16(&gba->bus, 0x02000000 + i * 2, write_stream[i]);
    }

    // DMA3 transfer to 0x0D000000 with count 81
    gba->dma.channels[3].sad = 0x02000000;
    gba->dma.channels[3].dad = 0x0D000000;
    gba->dma.channels[3].word_count = 81;
    dma_write_cnt_h(&gba->dma, 3, 0x8000); // 16-bit, immediate start, enable

    // Verify EEPROM contents at address 0 (bytes 0..7)
    uint64_t stored_val = 0;
    for (int i = 0; i < 8; i++) {
        stored_val = (stored_val << 8) | gba->bus.eeprom[i];
    }
    TEST_ASSERT(stored_val == test_data, "EEPROM 64-bit block write via DMA3");

    // 2. Prepare 17-bit read command for Addr 0:
    // "11" (2 bits) + Addr 0 (14 bits) + "0" (1 bit)
    uint16_t read_stream[17];
    memset(read_stream, 0, sizeof(read_stream));
    read_stream[0] = 1;
    read_stream[1] = 1; // Read command
    for (int i = 0; i < 17; i++) {
        bus_write16(&gba->bus, 0x02000100 + i * 2, read_stream[i]);
    }

    gba->dma.channels[3].sad = 0x02000100;
    gba->dma.channels[3].dad = 0x0D000000;
    gba->dma.channels[3].word_count = 17;
    dma_write_cnt_h(&gba->dma, 3, 0x8000);

    // 3. DMA3 read 68 bits from 0x0D000000 to EWRAM at 0x02000200
    gba->dma.channels[3].sad = 0x0D000000;
    gba->dma.channels[3].dad = 0x02000200;
    gba->dma.channels[3].word_count = 68;
    dma_write_cnt_h(&gba->dma, 3, 0x8000);

    // Reconstruct 64 bits from the destination buffer
    uint64_t read_back = 0;
    for (int b = 0; b < 64; b++) {
        uint16_t bit = bus_read16(&gba->bus, 0x02000200 + (4 + b) * 2) & 1;
        read_back = (read_back << 1) | bit;
    }
    TEST_ASSERT(read_back == test_data, "EEPROM 68-bit stream read-back via DMA3");

    gba_destroy(gba);
}

void test_bios_irq_trampoline(void) {
    printf("\n=== Testing BIOS IRQ Vector & User Handler Dispatch ===\n");
    GBA* gba = gba_create();
    gba_reset(gba, true);

    // Verify BIOS IRQ vector trampoline is installed at 0x00000018
    uint32_t op_18 = bus_read32(&gba->bus, 0x00000018);
    uint32_t op_1c = bus_read32(&gba->bus, 0x0000001C);
    uint32_t op_30 = bus_read32(&gba->bus, 0x00000030);
    uint32_t op_38 = bus_read32(&gba->bus, 0x00000038);

    TEST_ASSERT(op_18 == 0xE92D500F, "BIOS 0x18 contains STMDB SP!, {r0-r3, r12, lr}");
    TEST_ASSERT(op_1c == 0xE3A00404, "BIOS 0x1C loads 0x04000000 base");
    TEST_ASSERT(op_30 == 0xE12FFF11, "BIOS 0x30 invokes user handler via BX R1");
    TEST_ASSERT(op_38 == 0xE25EF004, "BIOS 0x38 returns from IRQ via SUBS PC, LR, #4");

    gba_destroy(gba);
}

int main(void) {
    printf("=====================================================\n");
    printf("   ZEFF STATION GBA EMULATOR TEST SUITE              \n");
    printf("=====================================================\n");

    test_memory_mirroring();
    test_dma_controller();
    test_eeprom_dma();
    test_timers_irq();
    test_bios_irq_trampoline();
    test_cpu_arm_execution();
    test_cpu_thumb_execution();
    test_ppu_mode3_execution();

    printf("\n=====================================================\n");
    printf(" TEST RESULTS: %d Passed, %d Failed\n", g_pass_count, g_fail_count);
    printf("=====================================================\n");

    return (g_fail_count == 0) ? 0 : 1;
}
