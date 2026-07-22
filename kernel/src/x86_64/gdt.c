#include "aurelian/gdt.h"

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_pointer gdt_ptr;

static void gdt_set_entry(int idx, uint32_t base, uint32_t limit, uint8_t access,
                          uint8_t granularity)
{
    gdt[idx].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[idx].base_low    = (uint16_t)(base & 0xFFFF);
    gdt[idx].base_middle = (uint8_t)((base >> 16) & 0xFF);
    gdt[idx].access      = access;
    gdt[idx].granularity = (uint8_t)((limit >> 16) & 0x0F);
    gdt[idx].granularity |= (granularity & 0xF0);
    gdt[idx].base_high   = (uint8_t)((base >> 24) & 0xFF);
}

static inline void gdt_flush(uint64_t gdtr)
{
    __asm__ volatile (
        "lgdt (%0)\n"
        "push $0x08\n"
        "lea 1f(%%rip), %%rax\n"
        "push %%rax\n"
        "lretq\n"
        "1:\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        : : "r"(gdtr) : "rax", "memory"
    );
}

void aurelion_gdt_init(void)
{
    gdt_ptr.limit = (uint16_t)(sizeof(struct gdt_entry) * GDT_ENTRIES - 1);
    gdt_ptr.base  = (uint64_t)&gdt;

    /* NULL descriptor */
    gdt_set_entry(GDT_NULL, 0, 0, 0, 0);

    /* Kernel code segment: base=0, limit=0xFFFFF, 64-bit code, executable, ring 0 */
    gdt_set_entry(GDT_KERNEL_CODE, 0, 0xFFFFF,
                  0x9A,   /* present, ring0, executable, readable */
                  0x20);  /* 4K granularity, 64-bit */

    /* Kernel data segment: base=0, limit=0xFFFFF, writable, ring 0 */
    gdt_set_entry(GDT_KERNEL_DATA, 0, 0xFFFFF,
                  0x92,   /* present, ring0, writable */
                  0x00);  /* 4K granularity */

    /* User code segment: base=0, limit=0xFFFFF, 64-bit code, executable, ring 3 */
    gdt_set_entry(GDT_USER_CODE, 0, 0xFFFFF,
                  0xFA,   /* present, ring3, executable, readable */
                  0x20);  /* 4K granularity, 64-bit */

    /* User data segment: base=0, limit=0xFFFFF, writable, ring 3 */
    gdt_set_entry(GDT_USER_DATA, 0, 0xFFFFF,
                  0xF2,   /* present, ring3, writable */
                  0x00);  /* 4K granularity */

    gdt_flush((uint64_t)&gdt_ptr);
}
