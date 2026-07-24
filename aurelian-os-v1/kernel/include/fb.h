/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/fb.h — double-buffered linear framebuffer graphics (Prism UI)
 *
 * All drawing targets an off-screen backbuffer (0x00RRGGBB per pixel);
 * fb_present() flushes it to the display.
 * ==========================================================================*/

#ifndef AURELIAN_FB_H
#define AURELIAN_FB_H

#include <stdint.h>

struct fb_info {
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t  bpp;
    uint8_t  type;      /* multiboot framebuffer_type (1 = RGB) */
};

static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

int  fb_init(const struct fb_info *info, uint32_t *backbuffer);
int  fb_ready(void);
int  fb_width(void);
int  fb_height(void);

void     fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t fb_get_pixel(uint32_t x, uint32_t y);
void     fb_fill_rect(int x, int y, int w, int h, uint32_t color);
void     fb_rounded_rect(int x, int y, int w, int h, int radius, uint32_t color);
void     fb_rect_border(int x, int y, int w, int h, int thick, uint32_t color);
void     fb_vgradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom);

void     fb_draw_char(int x, int y, char c, uint32_t color, int scale);
void     fb_draw_text(int x, int y, const char *s, uint32_t color, int scale);
int      fb_text_width(const char *s, int scale);
int      fb_print(int x, int y, const char *s, uint32_t color, int scale);      /* returns new x */
int      fb_print_uint(int x, int y, uint32_t v, uint32_t color, int scale);    /* returns new x */

void     fb_draw_cursor(int x, int y, uint32_t fill, uint32_t border);
void     fb_blit_raw32(const void *src, uint32_t sw, uint32_t sh);
void     fb_present(void);   /* flush backbuffer -> display */

#endif /* AURELIAN_FB_H */
