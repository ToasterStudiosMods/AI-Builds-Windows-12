/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * pic.c — 8259A Programmable Interrupt Controller (master + slave)
 * ==========================================================================*/

#include "pic.h"
#include "interrupts.h"
#include "io.h"

#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1

#define ICW1_INIT  0x11   /* init + expect ICW4                 */
#define ICW4_8086  0x01   /* 8086/88 mode                       */
#define PIC_EOI    0x20   /* end-of-interrupt command           */

void pic_remap(void)
{
    /* ICW1: start init sequence (cascade, expect ICW4). */
    outb(PIC1_CMD, ICW1_INIT); io_wait();
    outb(PIC2_CMD, ICW1_INIT); io_wait();

    /* ICW2: vector offsets — master 0x20-0x27, slave 0x28-0x2F. */
    outb(PIC1_DATA, 0x20); io_wait();
    outb(PIC2_DATA, 0x28); io_wait();

    /* ICW3: tell master a slave is at IRQ2 (bit 2); tell slave its id (2). */
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();

    /* ICW4: 8086 mode. */
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    /* Mask every line; drivers unmask their own via irq_unmask(). */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void irq_unmask(uint8_t irq)
{
    uint16_t port;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq  = (uint8_t)(irq - 8);
        /* Ensure the cascade line (IRQ2) is open so slave IRQs reach the CPU. */
        outb(PIC1_DATA, (uint8_t)(inb(PIC1_DATA) & ~(1u << 2)));
    }
    outb(port, (uint8_t)(inb(port) & ~(1u << irq)));
}

void pic_eoi(uint8_t irq)
{
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);   /* slave first */
    outb(PIC1_CMD, PIC_EOI);       /* always the master */
}
