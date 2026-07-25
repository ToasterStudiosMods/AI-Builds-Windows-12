/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/fb.h — double-buffered compositor primitives (Prism UI)
 *
 * All drawing targets an off-screen backbuffer of 0x00RRGGBB pixels;
 * fb_present() flushes it to the display. Everything is resolution
 * independent: query fb_width()/fb_height() and lay out from those.
 * ==========================================================================*/

#ifndef AURELIAN_FB_H
#define AURELIAN_FB_H

#include <stdint.h>

/* Largest mode we can composite (backbuffer + background cache are sized for
 * this). 1920x1080 covers every common VM/monitor mode. */
#define FB_MAX_W 1920
#define FB_MAX_H 1080
#define FB_MAX_PX ((uint32_t)FB_MAX_W * FB_MAX_H)

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

/* Blend src over dst with alpha 0..255. */
static inline uint32_t blend(uint32_t dst, uint32_t src, uint8_t a)
{
    uint32_t dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
    uint32_t sr = (src >> 16) & 0xFF, sg = (src >> 8) & 0xFF, sb = src & 0xFF;
    uint32_t r = dr + ((sr - dr) * a + 127) / 255;
    uint32_t g = dg + ((sg - dg) * a + 127) / 255;
    uint32_t b = db + ((sb - db) * a + 127) / 255;
    return (r << 16) | (g << 8) | b;
}

/* Lighten/darken by `amt` (0..255) — for hover/pressed states. */
static inline uint32_t shade(uint32_t c, int amt)
{
    int r = (int)((c >> 16) & 0xFF) + amt;
    int g = (int)((c >> 8) & 0xFF) + amt;
    int b = (int)(c & 0xFF) + amt;
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* `backbuffer` and `bgbuffer` must each hold width*height uint32 pixels; the
 * kernel allocates them from free physical memory rather than .bss. */
int  fb_init(const struct fb_info *info, uint32_t *backbuffer, uint32_t *bgbuffer,
             uint32_t *fadebuffer);
int  fb_ready(void);
int  fb_width(void);
int  fb_height(void);

/* --- pixels / rects --- */
void     fb_put_pixel(int x, int y, uint32_t color);
uint32_t fb_get_pixel(int x, int y);
void     fb_fill(int x, int y, int w, int h, uint32_t color);
void     fb_fill_a(int x, int y, int w, int h, uint32_t color, uint8_t alpha);
void     fb_round(int x, int y, int w, int h, int r, uint32_t color);
void     fb_round_a(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha);
void     fb_round_border(int x, int y, int w, int h, int r, int t, uint32_t color, uint8_t alpha);
void     fb_border(int x, int y, int w, int h, int t, uint32_t color);
void     fb_vgrad(int x, int y, int w, int h, uint32_t top, uint32_t bottom);
/* Soft drop shadow behind a rounded rect. */
void     fb_shadow(int x, int y, int w, int h, int r, int spread);

/* --- text (8x16 font; scale 1 = 8x16 px, scale 2 = 16x32 px) --- */
void     fb_glyph(int x, int y, char c, uint32_t color, int scale);
int      fb_text(int x, int y, const char *s, uint32_t color, int scale);   /* -> end x */
int      fb_text_w(const char *s, int scale);
int      fb_num(int x, int y, uint32_t v, uint32_t color, int scale);       /* -> end x */
int      fb_num2(int x, int y, uint32_t v, uint32_t color, int scale);      /* 0-padded 2 digits */
/* Text centred in a box. */
void     fb_text_c(int x, int y, int w, const char *s, uint32_t color, int scale);

/* --- background / wallpaper --- */
/* Upscale a raw 32-bit BGRX image to fill the screen, into the background
 * cache. Also builds the blurred copy used for Mica sampling. */
void     fb_set_wallpaper(const void *src, uint32_t sw, uint32_t sh);
void     fb_set_wallpaper_gradient(uint32_t top, uint32_t bottom);
/* Copy the cached background into the backbuffer (start of each frame). */
void     fb_draw_background(void);
/* Snapshot the current background so the next fb_set_wallpaper() cross-fades
 * into it; fb_fade_step() advances the blend and returns 1 while active. */
void     fb_fade_begin(void);
int      fb_fade_step(int delta);
int      fb_fade_active(void);
/* Sample the blurred wallpaper at a screen coordinate — the Mica base. */
uint32_t fb_mica_at(int x, int y);
/* Mica-filled rounded rect: blurred wallpaper tinted with `tint`. */
void     fb_mica_round(int x, int y, int w, int h, int r, uint32_t tint, uint8_t alpha);

void     fb_cursor(int x, int y);
void     fb_present(void);

#endif /* AURELIAN_FB_H */
