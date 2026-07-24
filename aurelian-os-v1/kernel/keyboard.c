/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * keyboard.c — PS/2 keyboard driver, scancode set 1 (A3, contributed by DeepSeek)
 *
 * Integrated with one fix vs. the original: shift is tracked with per-key
 * booleans instead of a counter. PS/2 typematic repeat sends repeated MAKE
 * codes while a key is held but only one BREAK on release; a counter would be
 * incremented on every repeat and left stuck > 0, jamming Shift on. Booleans
 * are idempotent under repeat.
 * ==========================================================================*/

#include <stdint.h>
#include "io.h"
#include "interrupts.h"
#include "input.h"
#include "keyboard.h"

/* Modifier state (per physical key, so typematic repeat is idempotent). */
static uint8_t left_shift  = 0;
static uint8_t right_shift = 0;
static uint8_t extended    = 0;

/* US QWERTY scancode set 1 — unshifted ASCII. Non-printable keys map to 0. */
static const char unshifted_ascii[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x0C] = '-', [0x0D] = '=', [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1A] = '[', [0x1B] = ']', [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l', [0x27] = ';',
    [0x28] = '\'', [0x29] = '`', [0x2B] = '\\',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
    [0x31] = 'n', [0x32] = 'm', [0x33] = ',', [0x34] = '.', [0x35] = '/',
    [0x37] = '*', [0x39] = ' ',
    [0x47] = '7', [0x48] = '8', [0x49] = '9', [0x4A] = '-',
    [0x4B] = '4', [0x4C] = '5', [0x4D] = '6', [0x4E] = '+',
    [0x4F] = '1', [0x50] = '2', [0x51] = '3', [0x52] = '0', [0x53] = '.',
};

/* US QWERTY scancode set 1 — shifted ASCII. */
static const char shifted_ascii[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%',
    [0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')',
    [0x0C] = '_', [0x0D] = '+', [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T',
    [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P',
    [0x1A] = '{', [0x1B] = '}', [0x1C] = '\n',
    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G',
    [0x23] = 'H', [0x24] = 'J', [0x25] = 'K', [0x26] = 'L', [0x27] = ':',
    [0x28] = '"', [0x29] = '~', [0x2B] = '|',
    [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V', [0x30] = 'B',
    [0x31] = 'N', [0x32] = 'M', [0x33] = '<', [0x34] = '>', [0x35] = '?',
    [0x37] = '*', [0x39] = ' ',
    [0x47] = '7', [0x48] = '8', [0x49] = '9', [0x4A] = '-',
    [0x4B] = '4', [0x4C] = '5', [0x4D] = '6', [0x4E] = '+',
    [0x4F] = '1', [0x50] = '2', [0x51] = '3', [0x52] = '0', [0x53] = '.',
};

/*
 * IRQ 1 handler. Reads the byte from port 0x60, translates it, pushes an
 * input_event. EOI is sent by the common dispatcher after this returns.
 */
static void keyboard_handler(void)
{
    uint8_t byte = inb(0x60);

    /* E0 is an extended-key prefix; consume it and wait for the next byte. */
    if (byte == 0xE0) {
        extended = 1;
        return;
    }

    uint8_t keycode = byte & 0x7F;
    int     make    = !(byte & 0x80);

    /* Track the two shift keys (ignore E0-prefixed duplicates for shift). */
    if (!extended) {
        if (keycode == 0x2A) left_shift  = (uint8_t)make;
        if (keycode == 0x36) right_shift = (uint8_t)make;
    }

    int shifted = left_shift || right_shift;

    struct input_event ev = {
        .type    = make ? INPUT_KEY_DOWN : INPUT_KEY_UP,
        .keycode = keycode,
        .ascii   = make ? (shifted ? shifted_ascii[keycode]
                                   : unshifted_ascii[keycode]) : 0,
        .buttons = 0,
        .dx      = 0,
        .dy      = 0,
    };
    input_push(&ev);

    extended = 0;   /* the E0 prefix (if any) has been consumed */
}

void keyboard_init(void)
{
    left_shift = right_shift = extended = 0;
    irq_install(1, keyboard_handler);
    irq_unmask(1);
}
