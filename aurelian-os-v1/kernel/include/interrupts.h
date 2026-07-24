/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/interrupts.h — IDT / IRQ public interface (the frozen driver contract)
 * ==========================================================================*/

#ifndef AURELIAN_INTERRUPTS_H
#define AURELIAN_INTERRUPTS_H

#include <stdint.h>

typedef void (*irq_handler_t)(void);

/* Build the IDT, install CPU exception + IRQ stubs, remap the 8259 PIC to
 * vectors 0x20-0x2F, and load the IDT. All IRQ lines start masked; interrupts
 * stay disabled until interrupts_enable(). */
void idt_init(void);

/* Register a handler for hardware IRQ 0-15. The common dispatcher calls fn()
 * then issues the PIC EOI, so handlers must NOT send EOI themselves. Handlers
 * run with interrupts disabled and must be short. */
void irq_install(uint8_t irq, irq_handler_t fn);

/* Unmask a single IRQ line on the PIC (call after installing its handler). */
void irq_unmask(uint8_t irq);

/* Execute `sti`. */
void interrupts_enable(void);

#endif /* AURELIAN_INTERRUPTS_H */
