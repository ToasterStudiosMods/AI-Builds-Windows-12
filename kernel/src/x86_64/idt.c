#include "aurelian/idt.h"
#include "aurelian/serial.h"

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_pointer idt_ptr;

/* Default exception handler that prints the vector number and halts */
static void default_exception_handler(uint64_t vector)
{
    aurelion_serial_write("EXCEPTION: vector ");
    /* Print vector number (simple itoa) */
    char buf[4] = {0};
    uint64_t v = vector;
    int i = 0;
    if (v == 0) {
        aurelion_serial_putc('0');
    } else {
        while (v > 0 && i < 3) {
            buf[i++] = '0' + (char)(v % 10);
            v /= 10;
        }
        for (int j = i - 1; j >= 0; j--) {
            aurelion_serial_putc(buf[j]);
        }
    }
    aurelion_serial_write("\n");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

/* Default ISR stubs - each one calls the generic handler with the vector number */
#define ISR_NOERR(n) \
    static void isr_##n(void) { default_exception_handler(n); }

#define ISR_ERR(n) \
    static void isr_##n(void) { default_exception_handler(n); }

/* Generate ISR stubs for all 32 CPU exception vectors */
ISR_NOERR(0)   /* Division Error */
ISR_NOERR(1)   /* Debug */
ISR_NOERR(2)   /* NMI */
ISR_NOERR(3)   /* Breakpoint */
ISR_NOERR(4)   /* Overflow */
ISR_NOERR(5)   /* Bound Range Exceeded */
ISR_NOERR(6)   /* Invalid Opcode */
ISR_ERR(7)     /* Device Not Available */
ISR_ERR(8)     /* Double Fault */
ISR_NOERR(9)   /* Coprocessor Segment Overrun */
ISR_ERR(10)    /* Invalid TSS */
ISR_ERR(11)    /* Segment Not Present */
ISR_ERR(12)    /* Stack-Segment Fault */
ISR_ERR(13)    /* General Protection Fault */
ISR_ERR(14)    /* Page Fault */
ISR_NOERR(15)  /* Reserved */
ISR_NOERR(16)  /* x87 FPU Error */
ISR_ERR(17)    /* Alignment Check */
ISR_NOERR(18)  /* Machine Check */
ISR_NOERR(19)  /* SIMD Floating-Point */
ISR_NOERR(20)  /* Virtualization */
ISR_NOERR(21)  /* Control Protection */
ISR_NOERR(22)  /* Reserved */
ISR_NOERR(23)  /* Reserved */
ISR_NOERR(24)  /* Reserved */
ISR_NOERR(25)  /* Reserved */
ISR_NOERR(26)  /* Reserved */
ISR_NOERR(27)  /* Reserved */
ISR_NOERR(28)  /* Reserved */
ISR_NOERR(29)  /* Reserved */
ISR_NOERR(30)  /* Reserved */
ISR_NOERR(31)  /* Reserved */

/* Function pointers array for all ISR stubs */
static void *isr_table[32] = {
    isr_0,  isr_1,  isr_2,  isr_3,  isr_4,  isr_5,  isr_6,  isr_7,
    isr_8,  isr_9,  isr_10, isr_11, isr_12, isr_13, isr_14, isr_15,
    isr_16, isr_17, isr_18, isr_19, isr_20, isr_21, isr_22, isr_23,
    isr_24, isr_25, isr_26, isr_27, isr_28, isr_29, isr_30, isr_31,
};

static void idt_set_entry(int idx, uint64_t handler, uint16_t selector,
                          uint8_t type_attr)
{
    idt[idx].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[idx].selector    = selector;
    idt[idx].ist         = 0;
    idt[idx].type_attr   = type_attr;
    idt[idx].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[idx].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[idx].reserved    = 0;
}

static inline void idt_flush(uint64_t idtr)
{
    __asm__ volatile ("lidt (%0)" : : "r"(idtr) : "memory");
}

void aurelion_idt_init(void)
{
    idt_ptr.limit = (uint16_t)(sizeof(struct idt_entry) * IDT_ENTRIES - 1);
    idt_ptr.base  = (uint64_t)&idt;

    /* Zero out all entries */
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_entry(i, 0, 0x08, 0x8E);
    }

    /* Set up CPU exception handlers (interrupt gate, ring 0) */
    for (int i = 0; i < 32; i++) {
        idt_set_entry(i, (uint64_t)isr_table[i], 0x08, 0x8E);
    }

    idt_flush((uint64_t)&idt_ptr);

    /* Enable interrupts now that IDT is ready */
    __asm__ volatile ("sti");
}
