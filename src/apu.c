#include "apu.h"
#include "gba.h"
#include "bus.h"
#include <string.h>
#include <stdlib.h>

static const uint8_t duty_patterns[4][8] = {
    { 0, 0, 0, 0, 0, 0, 0, 1 }, // 12.5%
    { 1, 0, 0, 0, 0, 0, 0, 1 }, // 25%
    { 1, 0, 0, 0, 0, 1, 1, 1 }, // 50%
    { 0, 1, 1, 1, 1, 1, 1, 0 }  // 75%
};

void apu_init(APU* apu, GBA* gba) {
    memset(apu, 0, sizeof(APU));
    apu->gba = gba;
    apu->soundbias = 0x0200;
    apu->ch4.lfsr = 0x7FFF;
}

void apu_reset(APU* apu) {
    memset(&apu->ch1, 0, sizeof(apu->ch1));
    memset(&apu->ch2, 0, sizeof(apu->ch2));
    memset(&apu->ch3, 0, sizeof(apu->ch3));
    memset(&apu->ch4, 0, sizeof(apu->ch4));
    memset(&apu->dsound_a, 0, sizeof(apu->dsound_a));
    memset(&apu->dsound_b, 0, sizeof(apu->dsound_b));

    apu->soundcnt_l = 0;
    apu->soundcnt_h = 0;
    apu->soundcnt_x = 0;
    apu->soundbias = 0x0200;
    apu->ch4.lfsr = 0x7FFF;
    apu->sample_count = 0;
    apu->sample_accumulator = 0;
}

void apu_write_fifo(APU* apu, int ch, int8_t sample) {
    DirectSound* ds = (ch == 0) ? &apu->dsound_a : &apu->dsound_b;
    if (ds->fifo_count < 32) {
        ds->fifo[ds->fifo_tail] = sample;
        ds->fifo_tail = (ds->fifo_tail + 1) % 32;
        ds->fifo_count++;
    }
}

void apu_on_timer_overflow(APU* apu, int timer_index) {
    // Check DirectSound A
    if (apu->dsound_a.timer_select == timer_index) {
        if (apu->dsound_a.fifo_count > 0) {
            apu->dsound_a.current_sample = apu->dsound_a.fifo[apu->dsound_a.fifo_head];
            apu->dsound_a.fifo_head = (apu->dsound_a.fifo_head + 1) % 32;
            apu->dsound_a.fifo_count--;
        }
        if (apu->dsound_a.fifo_count <= 16) {
            dma_trigger_sound_fifo(&apu->gba->dma, 0);
        }
    }

    // Check DirectSound B
    if (apu->dsound_b.timer_select == timer_index) {
        if (apu->dsound_b.fifo_count > 0) {
            apu->dsound_b.current_sample = apu->dsound_b.fifo[apu->dsound_b.fifo_head];
            apu->dsound_b.fifo_head = (apu->dsound_b.fifo_head + 1) % 32;
            apu->dsound_b.fifo_count--;
        }
        if (apu->dsound_b.fifo_count <= 16) {
            dma_trigger_sound_fifo(&apu->gba->dma, 1);
        }
    }
}

void apu_write16(APU* apu, uint32_t addr, uint16_t val) {
    uint32_t offset = addr & 0x3FF;
    switch (offset) {
        case REG_SOUND1CNT_L:
            apu->ch1.sweep_shift = val & 0x7;
            apu->ch1.sweep_negate = (val & (1 << 3)) != 0;
            apu->ch1.sweep_period = (val >> 4) & 0x7;
            break;
        case REG_SOUND1CNT_H:
            apu->ch1.length = 64 - (val & 0x3F);
            apu->ch1.duty = (val >> 6) & 0x3;
            apu->ch1.env_period = (val >> 8) & 0x7;
            apu->ch1.env_direction = (val & (1 << 11)) != 0;
            apu->ch1.env_initial = (val >> 12) & 0xF;
            break;
        case REG_SOUND1CNT_X:
            apu->ch1.freq = val & 0x7FF;
            apu->ch1.length_enable = (val & (1 << 14)) != 0;
            if (val & (1 << 15)) {
                apu->ch1.enabled = true;
                apu->ch1.env_volume = apu->ch1.env_initial;
                apu->ch1.env_timer = apu->ch1.env_period;
                apu->ch1.timer = (2048 - apu->ch1.freq) * 4;
            }
            break;

        case REG_SOUND2CNT_L:
            apu->ch2.length = 64 - (val & 0x3F);
            apu->ch2.duty = (val >> 6) & 0x3;
            apu->ch2.env_period = (val >> 8) & 0x7;
            apu->ch2.env_direction = (val & (1 << 11)) != 0;
            apu->ch2.env_initial = (val >> 12) & 0xF;
            break;
        case REG_SOUND2CNT_H:
            apu->ch2.freq = val & 0x7FF;
            apu->ch2.length_enable = (val & (1 << 14)) != 0;
            if (val & (1 << 15)) {
                apu->ch2.enabled = true;
                apu->ch2.env_volume = apu->ch2.env_initial;
                apu->ch2.env_timer = apu->ch2.env_period;
                apu->ch2.timer = (2048 - apu->ch2.freq) * 4;
            }
            break;

        case REG_SOUNDCNT_L:
            apu->soundcnt_l = val;
            break;
        case REG_SOUNDCNT_H:
            apu->soundcnt_h = val;
            apu->dsound_a.volume_full = (val & (1 << 2)) != 0;
            apu->dsound_b.volume_full = (val & (1 << 3)) != 0;
            apu->dsound_a.enable_right = (val & (1 << 8)) != 0;
            apu->dsound_a.enable_left = (val & (1 << 9)) != 0;
            apu->dsound_a.timer_select = (val & (1 << 10)) ? 1 : 0;
            if (val & (1 << 11)) {
                apu->dsound_a.fifo_count = 0;
                apu->dsound_a.fifo_head = 0;
                apu->dsound_a.fifo_tail = 0;
            }
            apu->dsound_b.enable_right = (val & (1 << 12)) != 0;
            apu->dsound_b.enable_left = (val & (1 << 13)) != 0;
            apu->dsound_b.timer_select = (val & (1 << 14)) ? 1 : 0;
            if (val & (1 << 15)) {
                apu->dsound_b.fifo_count = 0;
                apu->dsound_b.fifo_head = 0;
                apu->dsound_b.fifo_tail = 0;
            }
            break;
        case REG_SOUNDCNT_X:
            apu->soundcnt_x = val & 0x80;
            if (!(val & 0x80)) {
                apu_reset(apu);
            }
            break;
        case REG_SOUNDBIAS:
            apu->soundbias = val;
            break;
        default:
            break;
    }
}

void apu_write8(APU* apu, uint32_t addr, uint8_t val) {
    uint32_t offset = addr & 0x3FF;
    if (offset >= REG_FIFO_A && offset < REG_FIFO_A + 4) {
        apu_write_fifo(apu, 0, (int8_t)val);
        return;
    }
    if (offset >= REG_FIFO_B && offset < REG_FIFO_B + 4) {
        apu_write_fifo(apu, 1, (int8_t)val);
        return;
    }
    // General 16-bit register update
    uint16_t cur = apu_read16(apu, offset & ~1U);
    if (offset & 1) {
        apu_write16(apu, offset & ~1U, (cur & 0x00FF) | ((uint16_t)val << 8));
    } else {
        apu_write16(apu, offset & ~1U, (cur & 0xFF00) | val);
    }
}

uint16_t apu_read16(APU* apu, uint32_t addr) {
    uint32_t offset = addr & 0x3FF;
    switch (offset) {
        case REG_SOUNDCNT_L: return apu->soundcnt_l;
        case REG_SOUNDCNT_H: return apu->soundcnt_h;
        case REG_SOUNDCNT_X: {
            uint16_t status = apu->soundcnt_x & 0x80;
            if (apu->ch1.enabled) status |= (1 << 0);
            if (apu->ch2.enabled) status |= (1 << 1);
            if (apu->ch3.enabled) status |= (1 << 2);
            if (apu->ch4.enabled) status |= (1 << 3);
            return status;
        }
        case REG_SOUNDBIAS:  return apu->soundbias;
        default:             return *(uint16_t*)&apu->gba->bus.io[offset];
    }
}

uint8_t apu_read8(APU* apu, uint32_t addr) {
    uint16_t val = apu_read16(apu, addr & ~1U);
    return (addr & 1) ? (uint8_t)(val >> 8) : (uint8_t)(val & 0xFF);
}

void apu_step(APU* apu, int cycles) {
    if (!(apu->soundcnt_x & 0x80)) {
        return; // Sound disabled
    }

    // Advance Square 1
    if (apu->ch1.enabled) {
        if (apu->ch1.timer <= (uint32_t)cycles) {
            apu->ch1.duty_step = (apu->ch1.duty_step + 1) & 7;
            apu->ch1.timer += (2048 - apu->ch1.freq) * 4;
        } else {
            apu->ch1.timer -= cycles;
        }
    }

    // Advance Square 2
    if (apu->ch2.enabled) {
        if (apu->ch2.timer <= (uint32_t)cycles) {
            apu->ch2.duty_step = (apu->ch2.duty_step + 1) & 7;
            apu->ch2.timer += (2048 - apu->ch2.freq) * 4;
        } else {
            apu->ch2.timer -= cycles;
        }
    }

    // Audio sampling: ~380 cycles per sample @ 44.1 kHz
    apu->sample_accumulator += cycles;
    while (apu->sample_accumulator >= 380) {
        apu->sample_accumulator -= 380;

        int32_t left = 0;
        int32_t right = 0;

        // Mix DMG Channel 1
        if (apu->ch1.enabled) {
            int sample = duty_patterns[apu->ch1.duty][apu->ch1.duty_step] ? apu->ch1.env_volume : -apu->ch1.env_volume;
            if (apu->soundcnt_l & (1 << 8)) left += sample * 64;
            if (apu->soundcnt_l & (1 << 12)) right += sample * 64;
        }

        // Mix DMG Channel 2
        if (apu->ch2.enabled) {
            int sample = duty_patterns[apu->ch2.duty][apu->ch2.duty_step] ? apu->ch2.env_volume : -apu->ch2.env_volume;
            if (apu->soundcnt_l & (1 << 9)) left += sample * 64;
            if (apu->soundcnt_l & (1 << 13)) right += sample * 64;
        }

        // Mix DirectSound A
        int32_t sample_a = (int32_t)apu->dsound_a.current_sample * (apu->dsound_a.volume_full ? 512 : 256);
        if (apu->dsound_a.enable_left) left += sample_a;
        if (apu->dsound_a.enable_right) right += sample_a;

        // Mix DirectSound B
        int32_t sample_b = (int32_t)apu->dsound_b.current_sample * (apu->dsound_b.volume_full ? 512 : 256);
        if (apu->dsound_b.enable_left) left += sample_b;
        if (apu->dsound_b.enable_right) right += sample_b;

        // Clamp to 16-bit signed
        if (left > 32767) left = 32767;
        else if (left < -32768) left = -32768;

        if (right > 32767) right = 32767;
        else if (right < -32768) right = -32768;

        if (apu->sample_count < AUDIO_BUFFER_SIZE) {
            apu->sample_buffer[apu->sample_count * 2 + 0] = (int16_t)left;
            apu->sample_buffer[apu->sample_count * 2 + 1] = (int16_t)right;
            apu->sample_count++;
        }
    }
}

int apu_read_samples(APU* apu, int16_t* out_buffer, int max_samples) {
    int to_copy = (apu->sample_count < max_samples) ? apu->sample_count : max_samples;
    if (to_copy > 0) {
        memcpy(out_buffer, apu->sample_buffer, to_copy * 2 * sizeof(int16_t));
        if (to_copy < apu->sample_count) {
            memmove(apu->sample_buffer, &apu->sample_buffer[to_copy * 2], (apu->sample_count - to_copy) * 2 * sizeof(int16_t));
        }
        apu->sample_count -= to_copy;
    }
    return to_copy;
}
