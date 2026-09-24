#ifndef ZEFF_TIMER_H
#define ZEFF_TIMER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct GBA GBA;

typedef struct Timer {
    uint16_t reload;
    uint32_t counter; // internal counter
    uint16_t control;

    bool enabled;
    bool irq_enable;
    bool cascade;
    uint32_t prescaler_shift;
    uint32_t prescaler_mask;
    uint32_t prescaler_counter;
} Timer;

typedef struct TimerController {
    Timer timers[4];
    GBA* gba;
} TimerController;

void timer_init(TimerController* tc, GBA* gba);
void timer_reset(TimerController* tc);
void timer_step(TimerController* tc, int cycles);
void timer_write_reload(TimerController* tc, int index, uint16_t val);
void timer_write_control(TimerController* tc, int index, uint16_t val);
uint16_t timer_read_counter(TimerController* tc, int index);

#endif // ZEFF_TIMER_H
