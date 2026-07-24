/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/pic.h — 8259 PIC (internal to the interrupt subsystem)
 * ==========================================================================*/

#ifndef AURELIAN_PIC_H
#define AURELIAN_PIC_H

#include <stdint.h>

/* Remap master/slave PICs to vectors 0x20-0x27 / 0x28-0x2F, mask all lines. */
void pic_remap(void);

/* End-of-interrupt for the given IRQ (0-15). */
void pic_eoi(uint8_t irq);

#endif /* AURELIAN_PIC_H */
