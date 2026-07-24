/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/input.h — input event queue (drivers -> UI loop; frozen contract)
 * ==========================================================================*/

#ifndef AURELIAN_INPUT_H
#define AURELIAN_INPUT_H

#include <stdint.h>

enum input_type { INPUT_KEY_DOWN = 1, INPUT_KEY_UP = 2, INPUT_MOUSE = 3 };

struct input_event {
    uint8_t  type;      /* enum input_type                                   */
    uint8_t  keycode;   /* key events: scancode-set-1 make code              */
    char     ascii;     /* key events: translated ASCII, else 0              */
    uint8_t  buttons;   /* mouse events: bit0 L, bit1 R, bit2 M              */
    int16_t  dx, dy;    /* mouse events: relative motion, right/up positive  */
};

/* Producer, called from IRQ handlers. Drops the event if the ring is full. */
void input_push(const struct input_event *e);

/* Consumer, called from the main loop. Returns 1 and fills *out if an event
 * was available, else 0. */
int input_poll(struct input_event *out);

#endif /* AURELIAN_INPUT_H */
