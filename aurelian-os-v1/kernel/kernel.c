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
#include "mem.h"
#include "sched.h"
#include "e1000.h"
#include "net.h"
#include "pci.h"
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

struct mbi2_mmap {
    uint32_t type; uint32_t size;       /* type = 6 */
    uint32_t entry_size;
    uint32_t entry_version;
    /* entries follow */
};

struct mbi2_mmap_entry {
    uint64_t base;
    uint64_t len;
    uint32_t mem_type;                  /* 1 = available */
    uint32_t reserved;
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
#define MAX_WP 12
struct boot_facts {
    const char *loader;
    uint32_t    mem_upper_kib;
    int         have_fb;
    struct fb_info fb;
    int         nwp;
    uint64_t    wp_addr[MAX_WP];
    uint32_t    wp_size[MAX_WP];
    const struct mbi2_mmap *mmap;       /* memory map tag, if provided */
    uint64_t    mod_top;                /* highest byte used by any module */
};

static void parse_mbi2(uint64_t mbi2_info, struct boot_facts *out)
{
    out->loader = "unknown";
    out->mem_upper_kib = 0;
    out->have_fb = 0;
    out->nwp = 0;
    out->mmap = 0;
    out->mod_top = 0;

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
        case 3: {   /* boot modules named wp0, wp1, ... are wallpapers */
            const struct mbi2_module *m = (const struct mbi2_module *)tag;
            if (m->mod_end > out->mod_top) out->mod_top = m->mod_end;
            if (m->string[0] == 'w' && m->string[1] == 'p' && out->nwp < MAX_WP) {
                out->wp_addr[out->nwp] = m->mod_start;
                out->wp_size[out->nwp] = m->mod_end - m->mod_start;
                out->nwp++;
            }
            break;
        }
        case 6:
            out->mmap = (const struct mbi2_mmap *)tag;
            break;
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

/* ------------------------------------------------------------------ */
/* Early physical memory arena                                        */
/*                                                                    */
/* The compositor needs two full-screen 32-bpp buffers (up to ~8 MiB   */
/* each at 1920x1080). Putting those in .bss would inflate the kernel  */
/* image's memsz to ~18 MiB, which — together with the wallpaper boot  */
/* modules — is more than the loader could place, so the kernel never  */
/* started. Instead we carve them out of the largest free region the   */
/* loader reports, above both the kernel image and every module.       */
/* ------------------------------------------------------------------ */
extern char _kernel_end[];

static uint64_t arena_ptr, arena_end;

static void arena_init(const struct boot_facts *bf)
{
    uint64_t floor = (uint64_t)(uintptr_t)_kernel_end;
    if (bf->mod_top > floor) floor = bf->mod_top;
    floor = (floor + 0xFFFFu) & ~0xFFFFull;          /* 64 KiB guard + align  */

    uint64_t best_base = 0, best_len = 0;

    if (bf->mmap) {
        const uint8_t *e = (const uint8_t *)bf->mmap + sizeof(struct mbi2_mmap);
        const uint8_t *stop = (const uint8_t *)bf->mmap + bf->mmap->size;
        uint32_t es = bf->mmap->entry_size ? bf->mmap->entry_size : 24;
        for (; e + es <= stop; e += es) {
            const struct mbi2_mmap_entry *m = (const struct mbi2_mmap_entry *)e;
            if (m->mem_type != 1) continue;                 /* not usable      */
            uint64_t base = m->base, end = m->base + m->len;
            if (end > 0x100000000ull) end = 0x100000000ull;  /* identity map    */
            if (base < floor) base = floor;
            if (end <= base) continue;
            if (end - base > best_len) { best_base = base; best_len = end - base; }
        }
    }
    if (best_len == 0) {                    /* no map: trust mem_upper */
        best_base = floor;
        uint64_t top = 0x100000ull + (uint64_t)bf->mem_upper_kib * 1024ull;
        best_len = (top > floor) ? top - floor : 0;
    }
    arena_ptr = (best_base + 0xFFFu) & ~0xFFFull;
    arena_end = best_base + best_len;
}

void *phys_alloc(uint64_t bytes)
{
    bytes = (bytes + 0xFFFu) & ~0xFFFull;
    if (arena_ptr + bytes > arena_end) return 0;
    void *p = (void *)(uintptr_t)arena_ptr;
    arena_ptr += bytes;
    return p;
}

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

    /* Carve the compositor's buffers out of free physical memory. */
    arena_init(&bf);
    uint64_t px = (uint64_t)bf.fb.width * bf.fb.height;
    uint32_t *back = 0, *bgbuf = 0, *fadebuf = 0;
    if (bf.have_fb && px) {
        back  = (uint32_t *)phys_alloc(px * 4);
        bgbuf = (uint32_t *)phys_alloc(px * 4);
        fadebuf = (uint32_t *)phys_alloc(px * 4);
    }
    serial_write("[mem] arena "); serial_write_hex(arena_ptr);
    serial_write(" .. ");         serial_write_hex(arena_end);
    serial_write(back && bgbuf ? " (buffers ok)\n" : " (ALLOC FAILED)\n");

    if (bf.have_fb && fb_init(&bf.fb, back, bgbuf, fadebuf)) {
        serial_write("[drv] IDT + PIC + timer + keyboard + mouse...\n");
        idt_init();
        timer_init(100);
        keyboard_init();
        mouse_init();
        heap_init(4 * 1024 * 1024);
        sched_init("luma-shell");
        pci_scan();
        if (e1000_init()) net_init();
        interrupts_enable();
        serial_write("[drv] drivers up; starting Luma Shell (");
        serial_write_u64((uint64_t)bf.nwp); serial_write(" wallpapers).\n");
        shell_run(bf.nwp, bf.wp_addr, bf.wp_size, bf.mem_upper_kib);  /* no return */
    } else {
        serial_write("[fb] no usable framebuffer; VGA text.\n");
        text_fallback(&bf);
    }

    for (;;) __asm__ volatile ("hlt");
}
