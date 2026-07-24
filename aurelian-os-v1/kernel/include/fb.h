/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/fb.h — linear framebuffer graphics (Prism UI)
 *
 * A minimal software renderer for a 24/32-bpp RGB linear framebuffer supplied
 * by the Multiboot2 loader. Colours are 0x00RRGGBB.
 * ==========================================================================*/

#ifndef AURELIAN_FB_H
#define AURELIAN_FB_H

#include <stdint.h>

struct fb_info {
    uint64_t addr;      /* physical (identity-mapped) framebuffer base   */
    uint32_t pitch;     /* bytes per scanline                            */
    uint32_t width;     /* pixels                                        */
    uint32_t height;    /* pixels                                        */
    uint8_t  bpp;       /* bits per pixel (expect 24 or 32)              */
    uint8_t  type;      /* multiboot framebuffer_type (1 = RGB)          */
};

/* 0x00RRGGBB helpers. */
static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

int  fb_init(const struct fb_info *info);   /* returns 1 if usable RGB fb */
int  fb_ready(void);

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void fb_fill_rect(int x, int y, int w, int h, uint32_t color);
void fb_rounded_rect(int x, int y, int w, int h, int radius, uint32_t color);
void fb_rect_border(int x, int y, int w, int h, int thick, uint32_t color);
void fb_vgradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom);

/* 8x8 font text, integer-scaled (scale >= 1). */
void fb_draw_char(int x, int y, char c, uint32_t color, int scale);
void fb_draw_text(int x, int y, const char *s, uint32_t color, int scale);
int  fb_text_width(const char *s, int scale);

/* A little arrow cursor. */
void fb_draw_cursor(int x, int y, uint32_t fill, uint32_t border);

/* Flush CPU caches so pixels reach the emulated VRAM before we halt. */
void fb_present(void);

#endif /* AURELIAN_FB_H */
