/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * kernel.c — C entry point (kmain)
 *
 * Called from boot.S after the Multiboot2 loader jumps to `_start`. The mbi2
 * info pointer is passed as the first argument. v1 prints a boot banner and
 * basic boot info to the VGA text console, then halts. This is the
 * deliberately minimal, genuinely-bootable baseline of the Aurelion kernel.
 * ==========================================================================*/

#include "kernel.h"
#include "vga.h"
#include "string.h"
#include "gdt.h"
#include <stdarg.h>

/* ------------------------------------------------------------------ */
/* Multiboot2 info structure (subset). See spec at multiboot2.org.    */
/* ------------------------------------------------------------------ */
struct mbi2_tag {
    uint32_t type;
    uint32_t size;
};

struct mbi2_basic_meminfo {
    uint32_t type;     /* = 4 */
    uint32_t size;
    uint32_t mem_lower; /* in KiB */
    uint32_t mem_upper; /* in KiB */
};

struct mbi2_bootloader_name {
    uint32_t type;     /* = 2 */
    uint32_t size;
    char     string[]; /* NUL-terminated, padded */
};

/* ------------------------------------------------------------------ */
/* Console helpers                                                    */
/* ------------------------------------------------------------------ */
void kprint(const char *s)        { vga_puts(s); }
void kprintln(const char *s)      { vga_puts(s); vga_putc('\n'); }

/* Tiny printf: supports %s, %d, %u, %x, %c. No width/flags. */
void kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { vga_putc(*p); continue; }
        p++;
        switch (*p) {
        case 's': vga_puts(va_arg(ap, const char *)); break;
        case 'd': {
            char buf[12];
            int v = va_arg(ap, int);
            if (v < 0) { vga_putc('-'); v = -v; }
            vga_puts(uitoa((uint32_t)v, buf, 10));
            break;
        }
        case 'u': {
            char buf[12];
            vga_puts(uitoa(va_arg(ap, unsigned), buf, 10));
            break;
        }
        case 'x': {
            char buf[12];
            vga_puts(uitoa(va_arg(ap, unsigned), buf, 16));
            break;
        }
        case 'c': vga_putc((char)va_arg(ap, int)); break;
        case '%': vga_putc('%'); break;
        default: vga_putc('%'); vga_putc(*p); break;
        }
    }
    va_end(ap);
}

/* ------------------------------------------------------------------ */
/* Banner                                                             */
/* ------------------------------------------------------------------ */
static void print_banner(void)
{
    vga_set_color(vga_entry_color(VGA_YELLOW, VGA_BLACK));
    vga_puts(
        "                                                                   \n"
        "          ___                    ____                              \n"
        "         / _ |  ____  ____ _    / __ \\____  ____  _______  __     \n"
        "        / __ | / __ \\/ __ `/   / /_/ / __ \\/ __ \\/ ___/ / / /    \n"
        "       /_/ |_|/ /_/ / /_/ /   / _, _/ /_/ / / / / /__/ /_/ /       \n"
        "                \\___/\\__,_/   /_/ |_|\\____/_/ /_/\\___/\\__, /      \n"
        "                                                   /____/        \n"
        "                                                                   \n");
    vga_set_color(vga_entry_color(VGA_LIGHT_GREEN, VGA_BLACK));
    vga_puts("        Aurelian OS  ");
    vga_set_color(vga_entry_color(VGA_LIGHT_GREY, VGA_BLACK));
    vga_puts("· codename \"");
    vga_puts(AURELIAN_CODENAME);
    vga_puts("\"  · kernel ");
    vga_puts(AURELION_KERNEL);
    vga_puts("  · v");
    vga_puts(AURELIAN_VERSION);
    vga_putc('\n');
    vga_set_color(vga_entry_color(VGA_DARK_GREY, VGA_BLACK));
    vga_puts(
        "        -------------------------------------------------------------\n");
    vga_set_color(vga_entry_color(VGA_LIGHT_GREY, VGA_BLACK));
}

/* ------------------------------------------------------------------ */
/* Walk the mbi2 tag list for basic info we care about.               */
/* ------------------------------------------------------------------ */
static void print_boot_info(uint64_t mbi2_info)
{
    const uint8_t *ptr = (const uint8_t *)mbi2_info;
    /* first 8 bytes: total size (u32) + reserved (u32) */
    uint32_t total = *(const uint32_t *)ptr;

    const struct mbi2_tag *tag =
        (const struct mbi2_tag *)(ptr + 8);

    const char *loader = "unknown";
    uint32_t mem_lower = 0, mem_upper = 0;
    int have_mem = 0;

    while ((const uint8_t *)tag < ptr + total) {
        switch (tag->type) {
        case 2: /* bootloader name */
            loader = ((const struct mbi2_bootloader_name *)tag)->string;
            break;
        case 4: { /* basic meminfo */
            const struct mbi2_basic_meminfo *m =
                (const struct mbi2_basic_meminfo *)tag;
            mem_lower = m->mem_lower;
            mem_upper = m->mem_upper;
            have_mem = 1;
            break;
        }
        default:
            break;
        }
        /* tags are 8-byte aligned */
        uint32_t sz = (tag->size + 7) & ~((uint32_t)7);
        tag = (const struct mbi2_tag *)((const uint8_t *)tag + sz);
    }

    kprintf("  bootloader : %s\n", loader);
    kprintf("  mbi2 size  : %u bytes\n", total);
    if (have_mem) {
        kprintf("  mem lower  : %u KiB\n", mem_lower);
        kprintf("  mem upper  : %u KiB (%u MiB)\n",
                mem_upper, mem_upper / 1024);
    }
    kprintln("");
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */
void kmain(uint64_t mbi2_info)
{
    vga_init();
    print_banner();

    kprintln("[boot] long mode active (x86-64, 4-level paging)");
    kprintln("[boot] initializing runtime GDT ...");
    gdt_init();
    kprintln("[boot] GDT loaded (64-bit, ring 0)");

    kprintln("[boot] reading multiboot2 info ...");
    print_boot_info(mbi2_info);

    kprintln("[ok]   Aurelion kernel reached idle loop.");
    kprintln("");
    kprintln("v1 baseline reached. Halting CPU.");
    kprintln("(Scheduler, filesystems, and Luma Shell are v2+ milestones.)");
}
