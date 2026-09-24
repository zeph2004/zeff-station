#include "timer.h"
#include "gba.h"
#include "apu.h"
#include "interrupt.h"
#include <string.h>

static const uint32_t prescaler_shifts[4] = { 0, 6, 8, 10 };
static const uint32_t prescaler_masks[4]  = { 0x0, 0x3F, 0xFF, 0x3FF };

void timer_init(TimerController* tc, GBA* gba) {
    memset(tc, 0, sizeof(TimerController));
    tc->gba = gba;
}

void timer_reset(TimerController* tc) {
    for (int i = 0; i < 4; i++) {
        tc->timers[i].reload = 0;
        tc->timers[i].counter = 0;
        tc->timers[i].control = 0;
        tc->timers[i].enabled = false;
        tc->timers[i].irq_enable = false;
        tc->timers[i].cascade = false;
        tc->timers[i].prescaler_shift = 0;
        tc->timers[i].prescaler_mask = 0;
        tc->timers[i].prescaler_counter = 0;
    }
}

void timer_write_reload(TimerController* tc, int index, uint16_t val) {
    tc->timers[index].reload = val;
}

void timer_write_control(TimerController* tc, int index, uint16_t val) {
    Timer* t = &tc->timers[index];
    bool was_enabled = t->enabled;
    t->control = val;

    uint8_t prescaler_code = val & 0x3;
    t->prescaler_shift = prescaler_shifts[prescaler_code];
    t->prescaler_mask = prescaler_masks[prescaler_code];
    t->cascade = (index > 0) && ((val & (1 << 2)) != 0);
    t->irq_enable = (val & (1 << 6)) != 0;
    t->enabled = (val & (1 << 7)) != 0;

    if (!was_enabled && t->enabled) {
        t->counter = t->reload;
        t->prescaler_counter = 0;
    }
}

uint16_t timer_read_counter(TimerController* tc, int index) {
    return (uint16_t)tc->timers[index].counter;
}

static void timer_overflow(TimerController* tc, int index) {
    Timer* t = &tc->timers[index];
    t->counter = t->reload;

    if (t->irq_enable) {
        interrupt_request(&tc->gba->interrupt, (uint16_t)(IRQ_TIMER0 << index));
    }

    if (index == 0 || index == 1) {
        apu_on_timer_overflow(&tc->gba->apu, index);
    }

    // Cascade to next timer if enabled
    if (index < 3) {
        Timer* next = &tc->timers[index + 1];
        if (next->enabled && next->cascade) {
            next->counter++;
            if (next->counter > 0xFFFF) {
                timer_overflow(tc, index + 1);
            }
        }
    }
}

void timer_step(TimerController* tc, int cycles) {
    for (int i = 0; i < 4; i++) {
        Timer* t = &tc->timers[i];
        if (!t->enabled || t->cascade) {
            continue; // Disabled or driven by previous timer
        }

        t->prescaler_counter += cycles;
        uint32_t ticks = t->prescaler_counter >> t->prescaler_shift;
        t->prescaler_counter &= t->prescaler_mask;

        if (ticks > 0) {
            t->counter += ticks;
            if (t->counter > 0xFFFF) {
                timer_overflow(tc, i);
            }
        }
    }
}
