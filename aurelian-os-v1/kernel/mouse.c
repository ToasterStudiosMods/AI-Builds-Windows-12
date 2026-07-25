/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * mouse.c — PS/2 mouse driver, 3-byte packets (A4, contributed by Gemini)
 * Reviewed + integrated as-is.
 * ==========================================================================*/

#include <stdint.h>
#include <stddef.h>
#include "io.h"
#include "serial.h"
#include "interrupts.h"
#include "input.h"
#include "mouse.h"

#define MOUSE_DATA_PORT   0x60
#define MOUSE_CMD_PORT    0x64
#define MOUSE_STATUS_PORT 0x64

/* Poll status port until the input buffer is empty (bit 1 == 0). */
static void mouse_wait_write(void)
{
    while ((inb(MOUSE_STATUS_PORT) & 2) != 0)
        io_wait();
}

/* Poll status port until the output buffer is full (bit 0 == 1). */
static void mouse_wait_read(void)
{
    while ((inb(MOUSE_STATUS_PORT) & 1) == 0)
        io_wait();
}

/* Send a command to the mouse (0xD4 prefix), then read the 0xFA ACK. */
static void mouse_send_command(uint8_t command)
{
    mouse_wait_write();
    outb(MOUSE_CMD_PORT, 0xD4);

    mouse_wait_write();
    outb(MOUSE_DATA_PORT, command);

    mouse_wait_read();
    inb(MOUSE_DATA_PORT);          /* consume ACK */
}

static uint8_t mouse_cycle = 0;
static uint8_t mouse_packet[3];

static void mouse_irq_handler(void)
{
    uint8_t data = inb(MOUSE_DATA_PORT);

    switch (mouse_cycle) {
    case 0:
        /* Bit 3 must be 1; otherwise we're out of sync — drop and stay at 0. */
        if ((data & 0x08) == 0)
            return;
        mouse_packet[0] = data;
        mouse_cycle++;
        break;

    case 1:
        mouse_packet[1] = data;
        mouse_cycle++;
        break;

    case 2:
        mouse_packet[2] = data;
        mouse_cycle = 0;

        /* Drop on X/Y overflow. */
        if ((mouse_packet[0] & 0xC0) != 0)
            return;

        struct input_event event;
        event.type    = INPUT_MOUSE;
        event.keycode = 0;
        event.ascii   = 0;
        event.buttons = mouse_packet[0] & 0x07;

        int16_t dx = mouse_packet[1];
        if ((mouse_packet[0] & 0x10) != 0)
            dx |= (int16_t)0xFF00;
        event.dx = dx;

        int16_t dy = mouse_packet[2];
        if ((mouse_packet[0] & 0x20) != 0)
            dy |= (int16_t)0xFF00;
        event.dy = dy;

        input_push(&event);
        break;
    }
}

void mouse_init(void)
{
    uint8_t status;

    /* 1. Enable the auxiliary (mouse) device. */
    mouse_wait_write();
    outb(MOUSE_CMD_PORT, 0xA8);

    /* 2. Read the controller config byte, enable IRQ12, clear mouse clock. */
    mouse_wait_write();
    outb(MOUSE_CMD_PORT, 0x20);
    mouse_wait_read();
    status = inb(MOUSE_DATA_PORT);

    status |= 2;      /* bit 1: enable IRQ12   */
    status &= ~32;    /* bit 5: enable clock   */

    mouse_wait_write();
    outb(MOUSE_CMD_PORT, 0x60);
    mouse_wait_write();
    outb(MOUSE_DATA_PORT, status);

    /* 3. Mouse defaults + enable data reporting. */
    mouse_send_command(0xF6);
    mouse_send_command(0xF4);

    /* 4. Register + unmask IRQ12. */
    irq_install(12, mouse_irq_handler);
    irq_unmask(12);
}
