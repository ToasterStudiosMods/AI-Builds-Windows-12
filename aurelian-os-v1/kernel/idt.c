/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * idt.c — 64-bit Interrupt Descriptor Table + interrupt dispatch
 *
 * Installs 32 CPU exception gates and 16 hardware-IRQ gates (vectors 0x20-0x2F
 * after the PIC remap). The low-level entry stubs live in arch/x86_64/isr.S and
 * all funnel into interrupt_common() below.
 * ==========================================================================*/

#include "interrupts.h"
#include "pic.h"
#include "serial.h"
#include <stdint.h>

/* 64-bit interrupt/trap gate descriptor (16 bytes). */
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr   idtr;
static irq_handler_t    irq_handlers[16];

/* 48 entry-stub addresses (vectors 0-47), provided by isr.S. */
extern uint64_t isr_stub_table[48];

static void idt_set_gate(int vec, uint64_t handler)
{
    idt[vec].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[vec].selector    = 0x08;         /* kernel 64-bit code selector */
    idt[vec].ist         = 0;
    idt[vec].type_attr   = 0x8E;         /* present, ring 0, interrupt gate */
    idt[vec].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[vec].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[vec].zero        = 0;
}

void idt_init(void)
{
    pic_remap();

    for (int i = 0; i < 48; i++)
        idt_set_gate(i, isr_stub_table[i]);

    idtr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr.base  = (uint64_t)(uintptr_t)&idt;
    __asm__ volatile ("lidt %0" : : "m"(idtr));

    serial_write("[idt] IDT loaded, PIC remapped (IRQs 0x20-0x2F, all masked)\n");
}

void irq_install(uint8_t irq, irq_handler_t fn)
{
    if (irq < 16)
        irq_handlers[irq] = fn;
}

void interrupts_enable(void)
{
    __asm__ volatile ("sti");
}

/* Called by every stub in isr.S with the vector and (real or dummy) error
 * code. Vectors 0-31 are CPU exceptions; 32-47 are hardware IRQs. */
void interrupt_common(uint64_t vec, uint64_t err)
{
    if (vec < 32) {
        serial_write("\n*** CPU EXCEPTION vector=");
        serial_write_u64(vec);
        serial_write(" err=");
        serial_write_hex(err);
        serial_write(" — halting ***\n");
        for (;;)
            __asm__ volatile ("cli; hlt");
    }

    uint8_t irq = (uint8_t)(vec - 32);
    if (irq < 16 && irq_handlers[irq])
        irq_handlers[irq]();
    pic_eoi(irq);
}
