/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * kernel.c — C entry point (kmain)
 *
 * Boots from a Multiboot2 loader (GRUB), validates the environment, brings up
 * the GDT + interrupts + PS/2 drivers, then hands control to the Luma Shell.
 * Falls back to a VGA text banner if no usable framebuffer is present.
 * ==========================================================================*/

#include "kernel.h"
#include "vga.h"
#include "string.h"
#include "gdt.h"
#include "fb.h"
#include "serial.h"
#include "interrupts.h"
#include "timer.h"
#include "keyboard.h"
#include "mouse.h"
#include "shell.h"
#include <stdarg.h>

/* ------------------------------------------------------------------ */
/* Multiboot2 info structures (subset). See spec at multiboot2.org.   */
/* ------------------------------------------------------------------ */
struct mbi2_tag { uint32_t type; uint32_t size; };

struct mbi2_bootloader_name { uint32_t type; uint32_t size; char string[]; };

struct mbi2_basic_meminfo {
    uint32_t type; uint32_t size;
    uint32_t mem_lower; uint32_t mem_upper;
};

struct mbi2_framebuffer {
    uint32_t type; uint32_t size;
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t  bpp;
    uint8_t  fb_type;
    uint16_t reserved;
};

struct mbi2_module {
    uint32_t type; uint32_t size;       /* type = 3 */
    uint32_t mod_start;
    uint32_t mod_end;
    char     string[];
};

/* ------------------------------------------------------------------ */
/* VGA text console helpers (fallback path)                           */
/* ------------------------------------------------------------------ */
void kprint(const char *s)   { vga_puts(s); }
void kprintln(const char *s) { vga_puts(s); vga_putc('\n'); }

void kprintf(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { vga_putc(*p); continue; }
        p++;
        switch (*p) {
        case 's': vga_puts(va_arg(ap, const char *)); break;
        case 'u': { char b[12]; vga_puts(uitoa(va_arg(ap, unsigned), b, 10)); break; }
        case 'x': { char b[12]; vga_puts(uitoa(va_arg(ap, unsigned), b, 16)); break; }
        case 'c': vga_putc((char)va_arg(ap, int)); break;
        case '%': vga_putc('%'); break;
        default:  vga_putc('%'); vga_putc(*p); break;
        }
    }
    va_end(ap);
}

/* ------------------------------------------------------------------ */
/* Multiboot2 parse                                                   */
/* ------------------------------------------------------------------ */
struct boot_facts {
    const char *loader;
    uint32_t    mem_upper_kib;
    int         have_fb;
    struct fb_info fb;
    int         have_wp;
    uint64_t    wp_addr;
    uint32_t    wp_size;
};

static int bf_str_eq(const char *a, const char *b)
{ while (*a && *a == *b) { a++; b++; } return *a == *b; }

static void parse_mbi2(uint64_t mbi2_info, struct boot_facts *out)
{
    out->loader = "unknown";
    out->mem_upper_kib = 0;
    out->have_fb = 0;
    out->have_wp = 0;

    const uint8_t *ptr = (const uint8_t *)(uintptr_t)mbi2_info;
    uint32_t total = *(const uint32_t *)ptr;
    const struct mbi2_tag *tag = (const struct mbi2_tag *)(ptr + 8);

    while ((const uint8_t *)tag < ptr + total && tag->type != 0) {
        switch (tag->type) {
        case 2:
            out->loader = ((const struct mbi2_bootloader_name *)tag)->string;
            break;
        case 4:
            out->mem_upper_kib = ((const struct mbi2_basic_meminfo *)tag)->mem_upper;
            break;
        case 8: {
            const struct mbi2_framebuffer *f = (const struct mbi2_framebuffer *)tag;
            out->fb.addr   = f->addr;
            out->fb.pitch  = f->pitch;
            out->fb.width  = f->width;
            out->fb.height = f->height;
            out->fb.bpp    = f->bpp;
            out->fb.type   = f->fb_type;
            out->have_fb   = 1;
            break;
        }
        case 3: {
            const struct mbi2_module *m = (const struct mbi2_module *)tag;
            if (!out->have_wp || bf_str_eq(m->string, "wallpaper")) {
                out->wp_addr = m->mod_start;
                out->wp_size = m->mod_end - m->mod_start;
                out->have_wp = 1;
            }
            break;
        }
        default: break;
        }
        uint32_t sz = (tag->size + 7) & ~7u;
        tag = (const struct mbi2_tag *)((const uint8_t *)tag + sz);
    }
}

/* ------------------------------------------------------------------ */
/* VGA text fallback (no usable framebuffer)                          */
/* ------------------------------------------------------------------ */
static void text_fallback(const struct boot_facts *bf)
{
    vga_init();
    vga_set_color(vga_entry_color(VGA_LIGHT_GREEN, VGA_BLACK));
    kprintln("Aurelian OS  -  Aurelion kernel v1.0.0-dev");
    vga_set_color(vga_entry_color(VGA_LIGHT_GREY, VGA_BLACK));
    kprintln("");
    kprintln("[boot] long mode active (x86-64, 4-level paging)");
    kprintln("[boot] GDT loaded (64-bit, ring 0)");
    kprintf("  bootloader : %s\n", bf->loader);
    kprintf("  mem upper  : %u KiB\n", bf->mem_upper_kib);
    kprintln("");
    kprintln("No usable linear framebuffer; the Luma Shell needs graphics mode.");
    kprintln("Halting CPU.");
}

/* Compositing backbuffer for the shell (3 MiB). */
static uint32_t g_backbuffer[1024 * 768];

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */
void kmain(uint64_t mbi2_info)
{
    serial_init();
    serial_write("\nAurelion kernel v1.0.0-dev\n");
    serial_write("[boot] long mode active; installing GDT...\n");
    gdt_init();
    serial_write("[boot] GDT loaded (64-bit, ring 0)\n");

    struct boot_facts bf;
    parse_mbi2(mbi2_info, &bf);

    serial_write("[boot] bootloader: "); serial_write(bf.loader); serial_write("\n");
    if (bf.have_fb) {
        serial_write("[fb] "); serial_write_u64(bf.fb.width);
        serial_write("x"); serial_write_u64(bf.fb.height);
        serial_write("x"); serial_write_u64(bf.fb.bpp);
        serial_write(" addr="); serial_write_hex(bf.fb.addr); serial_write("\n");
    }

    if (bf.have_fb && fb_init(&bf.fb, g_backbuffer)) {
        serial_write("[drv] IDT + PIC + timer + keyboard + mouse...\n");
        idt_init();
        timer_init(100);
        keyboard_init();
        mouse_init();
        interrupts_enable();
        serial_write("[drv] drivers up; starting Luma Shell.\n");
        shell_run(bf.have_wp, bf.wp_addr, bf.wp_size);   /* never returns */
    } else {
        serial_write("[fb] no usable framebuffer; VGA text.\n");
        text_fallback(&bf);
    }

    for (;;) __asm__ volatile ("hlt");
}
