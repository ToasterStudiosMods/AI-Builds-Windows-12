/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * kernel.c — C entry point (kmain)
 *
 * Boots from a Multiboot2 loader (GRUB). If the loader hands us a linear RGB
 * framebuffer, we render the Prism UI desktop; otherwise we fall back to the
 * VGA text console. Then we halt. Interactivity (input, a real compositor and
 * the Luma Shell) are v2+ milestones.
 * ==========================================================================*/

#include "kernel.h"
#include "vga.h"
#include "string.h"
#include "gdt.h"
#include "fb.h"
#include "serial.h"
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
/* Parse the Multiboot2 tag list.                                     */
/* ------------------------------------------------------------------ */
struct boot_facts {
    const char *loader;
    uint32_t    mem_upper_kib;
    int         have_fb;
    struct fb_info fb;
};

static void parse_mbi2(uint64_t mbi2_info, struct boot_facts *out)
{
    out->loader = "unknown";
    out->mem_upper_kib = 0;
    out->have_fb = 0;

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
        default: break;
        }
        uint32_t sz = (tag->size + 7) & ~7u;   /* tags are 8-byte aligned */
        tag = (const struct mbi2_tag *)((const uint8_t *)tag + sz);
    }
}

/* ------------------------------------------------------------------ */
/* Prism UI palette (0x00RRGGBB)                                      */
/* ------------------------------------------------------------------ */
#define C_WALL_TOP  0x00141230u   /* deep indigo   */
#define C_WALL_BOT  0x004A2A82u   /* violet        */
#define C_BAR_BG    0x00F0F2F8u
#define C_BAR_FG    0x00282A36u
#define C_ACCENT1   0x006366F1u   /* indigo-500    */
#define C_ACCENT2   0x00A855F7u   /* purple-500    */
#define C_CARD_BG   0x00FAFAFDu
#define C_CARD_FG   0x00262634u
#define C_CARD_SUB  0x006E6E82u
#define C_WHITE     0x00FFFFFFu
#define C_SHADOW    0x00100C22u
#define C_BTN_OUT   0x00E6E6EEu

/* draw a decimal number, return the x advance in pixels */
static int draw_uint(int x, int y, uint32_t v, uint32_t color, int scale)
{
    char buf[12]; int i = 0;
    if (v == 0) buf[i++] = '0';
    while (v > 0 && i < 11) { buf[i++] = (char)('0' + v % 10); v /= 10; }
    /* reverse */
    for (int a = 0, b = i - 1; a < b; a++, b--) { char t = buf[a]; buf[a] = buf[b]; buf[b] = t; }
    buf[i] = 0;
    fb_draw_text(x, y, buf, color, scale);
    return x + fb_text_width(buf, scale);
}

static int draw_str(int x, int y, const char *s, uint32_t color, int scale)
{
    fb_draw_text(x, y, s, color, scale);
    return x + fb_text_width(s, scale);
}

static void draw_desktop(const struct boot_facts *bf)
{
    int W = (int)bf->fb.width, H = (int)bf->fb.height;

    /* wallpaper */
    fb_vgradient(0, 0, W, H, C_WALL_TOP, C_WALL_BOT);

    /* top bar */
    int bar_h = 30;
    fb_fill_rect(0, 0, W, bar_h, C_BAR_BG);
    fb_rounded_rect(10, 7, 16, 16, 4, C_ACCENT2);      /* logo chip */
    draw_str(34, 8, "Aurelian OS", C_BAR_FG, 2);
    /* right side: Prism UI + clock */
    const char *clk = "12:00";
    int clk_w = fb_text_width(clk, 2);
    draw_str(W - clk_w - 14, 8, clk, C_BAR_FG, 2);
    const char *pui = "Prism UI";
    draw_str(W - clk_w - 14 - fb_text_width(pui, 2) - 24, 8, pui, C_CARD_SUB, 2);

    /* centered window card */
    int cw = 600, ch = 340;
    if (cw > W - 40) cw = W - 40;
    if (ch > H - 140) ch = H - 140;
    int cx = (W - cw) / 2;
    int cy = (H - ch) / 2 + 8;

    fb_rounded_rect(cx + 6, cy + 10, cw, ch, 16, C_SHADOW);   /* drop shadow */
    fb_rounded_rect(cx, cy, cw, ch, 16, C_CARD_BG);           /* card body  */

    /* title bar: rounded top, square bottom, accent */
    int tb_h = 46;
    fb_rounded_rect(cx, cy, cw, tb_h, 16, C_ACCENT1);
    fb_fill_rect(cx, cy + 20, cw, tb_h - 20, C_ACCENT1);
    draw_str(cx + 20, cy + 15, "Welcome to Aurelian OS", C_WHITE, 2);

    /* window control dots (top-right) */
    fb_rounded_rect(cx + cw - 66, cy + 17, 12, 12, 6, 0x00ED6A5Eu);
    fb_rounded_rect(cx + cw - 48, cy + 17, 12, 12, 6, 0x00F5BF4Fu);
    fb_rounded_rect(cx + cw - 30, cy + 17, 12, 12, 6, 0x0061C554u);

    /* body text */
    int bx = cx + 26, by = cy + tb_h + 22;
    draw_str(bx, by, "codename \"Luma\"", C_CARD_SUB, 2);            by += 30;
    draw_str(bx, by, "Aurelion kernel  v1.0.0-dev", C_CARD_FG, 2);  by += 30;
    {
        int x = draw_str(bx, by, "Framebuffer  ", C_CARD_FG, 2);
        x = draw_uint(x, by, bf->fb.width, C_ACCENT1, 2);
        x = draw_str(x, by, " x ", C_CARD_FG, 2);
        x = draw_uint(x, by, bf->fb.height, C_ACCENT1, 2);
        x = draw_str(x, by, " x ", C_CARD_FG, 2);
        x = draw_uint(x, by, bf->fb.bpp, C_ACCENT1, 2);
        draw_str(x, by, " bpp", C_CARD_FG, 2);
        by += 30;
    }
    draw_str(bx, by, "Graphical desktop online.", C_CARD_FG, 2);

    /* buttons */
    int btn_y = cy + ch - 66;
    fb_rounded_rect(bx, btn_y, 200, 42, 10, C_ACCENT1);
    draw_str(bx + (200 - fb_text_width("Get Started", 2)) / 2, btn_y + 13, "Get Started", C_WHITE, 2);
    fb_rounded_rect(bx + 216, btn_y, 130, 42, 10, C_BTN_OUT);
    draw_str(bx + 216 + (130 - fb_text_width("About", 2)) / 2, btn_y + 13, "About", C_CARD_FG, 2);

    /* dock */
    int dock_w = 300, dock_h = 54;
    int dx = (W - dock_w) / 2, dy = H - 72;
    fb_rounded_rect(dx + 4, dy + 5, dock_w, dock_h, 18, C_SHADOW);
    fb_rounded_rect(dx, dy, dock_w, dock_h, 18, 0x00ECEDF5u);
    uint32_t icons[5] = { 0x00EF4444u, 0x00F59E0Bu, 0x0022C55Eu, 0x003B82F6u, 0x00A855F7u };
    for (int i = 0; i < 5; i++)
        fb_rounded_rect(dx + 20 + i * 54, dy + 9, 36, 36, 9, icons[i]);

    /* cursor */
    fb_draw_cursor(W / 2 + 30, H / 2, C_WHITE, 0x00141414u);

    fb_present();
}

/* ------------------------------------------------------------------ */
/* VGA text fallback (no framebuffer available)                       */
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
    kprintln("No linear framebuffer from the loader; Prism UI needs graphics mode.");
    kprintln("v1 baseline reached. Halting CPU.");
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
        serial_write("[fb] addr=");   serial_write_hex(bf.fb.addr);
        serial_write(" pitch=");      serial_write_u64(bf.fb.pitch);
        serial_write(" ");            serial_write_u64(bf.fb.width);
        serial_write("x");            serial_write_u64(bf.fb.height);
        serial_write("x");            serial_write_u64(bf.fb.bpp);
        serial_write(" type=");       serial_write_u64(bf.fb.type);
        serial_write("\n");
    } else {
        serial_write("[fb] no framebuffer tag from loader\n");
    }

    if (bf.have_fb && fb_init(&bf.fb)) {
        serial_write("[fb] rendering Prism UI desktop...\n");
        draw_desktop(&bf);
        serial_write("[ok] desktop rendered. Halting.\n");
    } else {
        serial_write("[fb] falling back to VGA text console\n");
        text_fallback(&bf);
        serial_write("[ok] text banner shown. Halting.\n");
    }

    for (;;) __asm__ volatile ("hlt");
}
