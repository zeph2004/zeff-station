#include "keypad.h"
#include "gba.h"
#include "bus.h"
#include <string.h>

void keypad_init(Keypad* kp, GBA* gba) {
    memset(kp, 0, sizeof(Keypad));
    kp->gba = gba;
    kp->keyinput = 0x03FF;
}

void keypad_reset(Keypad* kp) {
    kp->keyinput = 0x03FF;
    kp->keycnt = 0;
}

void keypad_set_key(Keypad* kp, GbaKey key, bool pressed) {
    if (pressed) {
        kp->keyinput &= ~(1 << key);
    } else {
        kp->keyinput |= (1 << key);
    }
    *(uint16_t*)&kp->gba->bus.io[REG_KEYINPUT] = kp->keyinput;
    keypad_check_irq(kp);
}

void keypad_check_irq(Keypad* kp) {
    if (!(kp->keycnt & (1 << 14))) {
        return; // IRQ not enabled
    }

    uint16_t mask = kp->keycnt & 0x03FF;
    uint16_t pressed = (~kp->keyinput) & 0x03FF;
    bool and_cond = (kp->keycnt & (1 << 15)) != 0;

    bool triggered = false;
    if (and_cond) {
        if ((pressed & mask) == mask) triggered = true;
    } else {
        if (pressed & mask) triggered = true;
    }

    if (triggered) {
        interrupt_request(&kp->gba->interrupt, IRQ_KEYPAD);
    }
}
