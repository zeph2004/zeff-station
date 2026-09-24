#ifndef ZEFF_KEYPAD_H
#define ZEFF_KEYPAD_H

#include <stdint.h>
#include <stdbool.h>

typedef struct GBA GBA;

typedef enum {
    KEY_A      = 0,
    KEY_B      = 1,
    KEY_SELECT = 2,
    KEY_START  = 3,
    KEY_RIGHT  = 4,
    KEY_LEFT   = 5,
    KEY_UP     = 6,
    KEY_DOWN   = 7,
    KEY_R      = 8,
    KEY_L      = 9
} GbaKey;

typedef struct Keypad {
    uint16_t keyinput; // 0x04000130 (active low, 0 = pressed)
    uint16_t keycnt;   // 0x04000132
    GBA* gba;
} Keypad;

void keypad_init(Keypad* kp, GBA* gba);
void keypad_reset(Keypad* kp);
void keypad_set_key(Keypad* kp, GbaKey key, bool pressed);
void keypad_check_irq(Keypad* kp);

#endif // ZEFF_KEYPAD_H
