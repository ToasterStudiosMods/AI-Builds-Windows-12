/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * serial.c — COM1 (0x3F8) serial output, for CI/headless diagnostics
 * ==========================================================================*/

#include "serial.h"

#define COM1 0x3F8

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

void serial_init(void)
{
    outb(COM1 + 1, 0x00); /* disable interrupts        */
    outb(COM1 + 3, 0x80); /* enable DLAB               */
    outb(COM1 + 0, 0x03); /* divisor lo = 3 (38400)    */
    outb(COM1 + 1, 0x00); /* divisor hi                */
    outb(COM1 + 3, 0x03); /* 8N1                       */
    outb(COM1 + 2, 0xC7); /* enable + clear FIFOs      */
    outb(COM1 + 4, 0x0B); /* IRQs enabled, RTS/DSR set */
}

void serial_putc_pub(char c);
static void serial_putc(char c)
{
    while (!(inb(COM1 + 5) & 0x20))
        ;
    outb(COM1, (uint8_t)c);
}

void serial_write(const char *s)
{
    for (; *s; s++) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s);
    }
}

void serial_write_u64(uint64_t v)
{
    char buf[21];
    int i = 0;
    if (v == 0) { serial_putc('0'); return; }
    while (v > 0 && i < 20) { buf[i++] = (char)('0' + v % 10); v /= 10; }
    while (i--) serial_putc(buf[i]);
}

void serial_write_hex(uint64_t v)
{
    const char *hex = "0123456789ABCDEF";
    serial_write("0x");
    for (int shift = 60; shift >= 0; shift -= 4)
        serial_putc(hex[(v >> shift) & 0xF]);
}

void serial_write_hex8(uint8_t v)
{
    const char *hex = "0123456789ABCDEF";
    serial_putc_pub(hex[(v >> 4) & 0xF]);
    serial_putc_pub(hex[v & 0xF]);
}

void serial_putc_pub(char c) { serial_putc(c); }
