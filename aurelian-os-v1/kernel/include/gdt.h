/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/gdt.h — Global Descriptor Table (64-bit long mode)
 * ==========================================================================*/

#ifndef AURELIAN_GDT_H
#define AURELIAN_GDT_H

#include <stdint.h>

/* A legacy GDT entry (8 bytes), used for null/data descriptors. */
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

/* The GDTR register load structure (10 bytes in 64-bit mode). */
struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* Initialize the runtime GDT (null + 64-bit code + data) and load it. */
void gdt_init(void);

#endif /* AURELIAN_GDT_H */
