/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * arch/x86_64/gdt.c — runtime 64-bit Global Descriptor Table
 *
 * The boot.S path loads a minimal GDT to reach long mode. After kmain is
 * running we install a clean runtime GDT (null + 64-bit code + 64-bit data)
 * for correctness and to allow ring-3 segments to be added later.
 * ==========================================================================*/

#include "gdt.h"
#include "string.h"

/* 3 entries: null, 64-bit kernel code, 64-bit kernel data. */
static struct gdt_entry gdt[3];
static struct gdt_ptr   gdt_ptr;

static void gdt_set_entry(int i, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t gran)
{
    gdt[i].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[i].base_low    = (uint16_t)(base & 0xFFFF);
    gdt[i].base_mid    = (uint8_t)((base >> 16) & 0xFF);
    gdt[i].access      = access;
    gdt[i].granularity = (uint8_t)(((limit >> 16) & 0x0F) | (gran & 0xF0));
    gdt[i].base_high   = (uint8_t)((base >> 24) & 0xFF);
}

/* Load the GDT and reload segment registers. Implemented in gdt_flush.S. */
extern void gdt_flush(uint64_t gdt_ptr_addr);

void gdt_init(void)
{
    memset(gdt, 0, sizeof(gdt));

    /* null descriptor */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* 64-bit kernel code: present | ring0 | code | exec/read | L (64-bit).
     * access = 0x9A (present|ring0|code|exec/read),
     * gran   = 0xA0 (4K gran | 64-bit L flag set, limit 0xF bits). */
    gdt_set_entry(1, 0, 0xFFFF, 0x9A, 0xAF);

    /* 64-bit kernel data: present | ring0 | data | read/write. */
    gdt_set_entry(2, 0, 0xFFFF, 0x92, 0xCF);

    gdt_ptr.limit = (uint16_t)(sizeof(gdt) - 1);
    gdt_ptr.base  = (uint64_t)(unsigned long)&gdt;

    gdt_flush((uint64_t)(unsigned long)&gdt_ptr);
}
