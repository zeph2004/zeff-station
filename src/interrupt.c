#include "interrupt.h"
#include "gba.h"
#include "bus.h"
#include <string.h>

void interrupt_init(InterruptController* ic, GBA* gba) {
    memset(ic, 0, sizeof(InterruptController));
    ic->gba = gba;
}

void interrupt_reset(InterruptController* ic) {
    ic->ime = 0;
    ic->ie = 0;
    ic->if_ = 0;
}

void interrupt_request(InterruptController* ic, uint16_t irq_mask) {
    ic->if_ |= irq_mask;
    *(uint16_t*)&ic->gba->bus.io[REG_IF] = ic->if_;

    // CPU unhalts when an interrupt enabled in IE occurs (even if IME=0 or CPSR I-flag=1)
    if (ic->ie & ic->if_) {
        ic->gba->halted = false;
    }
}

void interrupt_check(InterruptController* ic) {
    if ((ic->ime & 1) && (ic->ie & ic->if_)) {
        if (!(ic->gba->cpu.cpsr & FLAG_I)) {
            ic->gba->halted = false;
        }
    }
}
