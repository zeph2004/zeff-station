#ifndef ZEFF_INTERRUPT_H
#define ZEFF_INTERRUPT_H

#include <stdint.h>
#include <stdbool.h>

typedef struct GBA GBA;

#define IRQ_VBLANK   (1 << 0)
#define IRQ_HBLANK   (1 << 1)
#define IRQ_VCOUNT   (1 << 2)
#define IRQ_TIMER0   (1 << 3)
#define IRQ_TIMER1   (1 << 4)
#define IRQ_TIMER2   (1 << 5)
#define IRQ_TIMER3   (1 << 6)
#define IRQ_SERIAL   (1 << 7)
#define IRQ_DMA0     (1 << 8)
#define IRQ_DMA1     (1 << 9)
#define IRQ_DMA2     (1 << 10)
#define IRQ_DMA3     (1 << 11)
#define IRQ_KEYPAD   (1 << 12)
#define IRQ_GAMEPAK  (1 << 13)

typedef struct InterruptController {
    uint16_t ime; // Interrupt Master Enable (bit 0)
    uint16_t ie;  // Interrupt Enable
    uint16_t if_; // Interrupt Request Flags
    GBA* gba;
} InterruptController;

void interrupt_init(InterruptController* ic, GBA* gba);
void interrupt_reset(InterruptController* ic);
void interrupt_request(InterruptController* ic, uint16_t irq_mask);
void interrupt_check(InterruptController* ic);

#endif // ZEFF_INTERRUPT_H
