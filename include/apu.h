#ifndef ZEFF_APU_H
#define ZEFF_APU_H

#include <stdint.h>
#include <stdbool.h>

typedef struct GBA GBA;

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_BUFFER_SIZE 2048

typedef struct DmgSquare {
    bool enabled;
    uint8_t duty;
    uint8_t duty_step;
    uint16_t length;
    bool length_enable;

    // Envelope
    uint8_t env_initial;
    uint8_t env_volume;
    bool env_direction;
    uint8_t env_period;
    uint8_t env_timer;

    // Frequency & Sweep (Ch 1)
    uint16_t freq;
    uint32_t timer;
    uint8_t sweep_period;
    uint8_t sweep_timer;
    uint8_t sweep_shift;
    bool sweep_negate;
    bool sweep_enabled;
    uint16_t sweep_shadow;
} DmgSquare;

typedef struct DmgWave {
    bool enabled;
    bool dac_enable;
    uint16_t length;
    bool length_enable;
    uint8_t volume_shift;
    uint16_t freq;
    uint32_t timer;
    uint8_t sample_pos;
    uint8_t wave_ram[16]; // 32 4-bit samples
} DmgWave;

typedef struct DmgNoise {
    bool enabled;
    uint16_t length;
    bool length_enable;

    // Envelope
    uint8_t env_initial;
    uint8_t env_volume;
    bool env_direction;
    uint8_t env_period;
    uint8_t env_timer;

    uint16_t lfsr;
    bool width_mode; // 7-bit vs 15-bit
    uint8_t divisor_code;
    uint8_t clock_shift;
    uint32_t timer;
} DmgNoise;

typedef struct DirectSound {
    int8_t fifo[32];
    int fifo_head;
    int fifo_tail;
    int fifo_count;
    int8_t current_sample;
    int timer_select; // 0 = Timer 0, 1 = Timer 1
    bool enable_right;
    bool enable_left;
    bool volume_full; // false = 50%, true = 100%
} DirectSound;

typedef struct APU {
    DmgSquare ch1;
    DmgSquare ch2;
    DmgWave ch3;
    DmgNoise ch4;

    DirectSound dsound_a;
    DirectSound dsound_b;

    // Control registers
    uint16_t soundcnt_l;
    uint16_t soundcnt_h;
    uint16_t soundcnt_x;
    uint16_t soundbias;

    // Master sample clock accumulator
    uint32_t sample_accumulator;
    uint32_t frame_seq_timer;
    uint8_t frame_seq_step;

    // Output sample buffer
    int16_t sample_buffer[AUDIO_BUFFER_SIZE * 2];
    int sample_count; // in stereo frames

    GBA* gba;
} APU;

void apu_init(APU* apu, GBA* gba);
void apu_reset(APU* apu);
void apu_step(APU* apu, int cycles);

void apu_write_fifo(APU* apu, int ch, int8_t sample);
void apu_on_timer_overflow(APU* apu, int timer_index);

void apu_write8(APU* apu, uint32_t addr, uint8_t val);
void apu_write16(APU* apu, uint32_t addr, uint16_t val);
uint8_t apu_read8(APU* apu, uint32_t addr);
uint16_t apu_read16(APU* apu, uint32_t addr);

int apu_read_samples(APU* apu, int16_t* out_buffer, int max_samples);

#endif // ZEFF_APU_H
