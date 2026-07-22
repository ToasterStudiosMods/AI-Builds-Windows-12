#ifndef AURELIAN_GDT_H
#define AURELIAN_GDT_H

#include <stdint.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

enum {
    GDT_NULL        = 0,
    GDT_KERNEL_CODE = 1,
    GDT_KERNEL_DATA = 2,
    GDT_USER_CODE   = 3,
    GDT_USER_DATA   = 4,
    GDT_ENTRIES     = 5,
};

void aurelion_gdt_init(void);

#endif
