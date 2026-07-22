#ifndef AURELIAN_IDT_H
#define AURELIAN_IDT_H

#include <stdint.h>

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;       /* Interrupt Stack Table offset (0 = don't use) */
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

enum {
    IDT_ENTRIES = 256,
};

void aurelion_idt_init(void);

#endif
