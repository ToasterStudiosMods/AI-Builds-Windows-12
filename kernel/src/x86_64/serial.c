#include "aurelian/serial.h"
#include <stddef.h>

enum {
    COM1    = 0x3F8,
    COM2    = 0x2F8,
    COM3    = 0x3E8,
    COM4    = 0x2E8,
};

enum {
    SERIAL_DATA         = 0,
    SERIAL_INT_ENABLE   = 1,
    SERIAL_FIFO_CTRL    = 2,
    SERIAL_LINE_CTRL    = 3,
    SERIAL_MODEM_CTRL   = 4,
    SERIAL_LINE_STATUS  = 5,
    SERIAL_MODEM_STATUS = 6,
    SERIAL_SCRATCH      = 7,
};

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void aurelion_serial_init(void)
{
    outb(COM1 + SERIAL_INT_ENABLE, 0x00);    /* Disable interrupts */
    outb(COM1 + SERIAL_LINE_CTRL,   0x80);    /* Enable DLAB */
    outb(COM1 + SERIAL_DATA,        0x03);    /* Baud divisor lo: 38400 */
    outb(COM1 + SERIAL_INT_ENABLE,  0x00);    /* Baud divisor hi */
    outb(COM1 + SERIAL_LINE_CTRL,   0x03);    /* 8 bits, no parity, 1 stop */
    outb(COM1 + SERIAL_FIFO_CTRL,   0xC7);    /* Enable FIFO, clear, 14-byte threshold */
    outb(COM1 + SERIAL_MODEM_CTRL,  0x0B);    /* IRQs enabled, RTS/DSR set */
}

static int serial_transmit_empty(void)
{
    return inb(COM1 + SERIAL_LINE_STATUS) & 0x20;
}

void aurelion_serial_putc(char c)
{
    while (!serial_transmit_empty())
        ;
    outb(COM1 + SERIAL_DATA, (uint8_t)c);
}

void aurelion_serial_write(const char *text)
{
    for (size_t i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            aurelion_serial_putc('\r');
        }
        aurelion_serial_putc(text[i]);
    }
}
