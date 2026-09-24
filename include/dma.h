#ifndef ZEFF_DMA_H
#define ZEFF_DMA_H

#include <stdint.h>
#include <stdbool.h>

typedef struct GBA GBA;

typedef enum {
    DMA_START_IMMEDIATE = 0,
    DMA_START_VBLANK = 1,
    DMA_START_HBLANK = 2,
    DMA_START_SPECIAL = 3
} DmaStartTiming;

typedef struct DmaChannel {
    uint32_t sad; // Source address
    uint32_t dad; // Destination address
    uint16_t word_count; // Transfer count
    uint16_t control;

    // Internal latch registers
    uint32_t internal_sad;
    uint32_t internal_dad;
    uint32_t internal_count;

    int dest_adj;
    int src_adj;
    bool repeat;
    bool is_32bit;
    DmaStartTiming start_timing;
    bool irq_on_finish;
    bool enabled;
    bool pending;
} DmaChannel;

typedef struct DMAController {
    DmaChannel channels[4];
    GBA* gba;
} DMAController;

void dma_init(DMAController* dma, GBA* gba);
void dma_reset(DMAController* dma);
void dma_write_cnt_h(DMAController* dma, int ch, uint16_t val);
void dma_check_trigger(DMAController* dma, DmaStartTiming timing);
void dma_step(DMAController* dma);
void dma_trigger_sound_fifo(DMAController* dma, int fifo_ch);

#endif // ZEFF_DMA_H
