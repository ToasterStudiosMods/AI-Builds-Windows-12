/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * fb.c — double-buffered software renderer
 *
 * All drawing targets an off-screen 32-bpp backbuffer (0x00RRGGBB per pixel);
 * fb_present() flushes it to the linear framebuffer. This gives flicker-free
 * compositing for the window manager.
 * ==========================================================================*/

#include "fb.h"
#include "font8x8.h"
#include "string.h"

static struct fb_info g_fb;
static uint32_t      *g_back;       /* backbuffer: g_fb.width * g_fb.height */
static uint8_t        g_ready;

int fb_init(const struct fb_info *info, uint32_t *backbuffer)
{
    g_fb   = *info;
    g_back = backbuffer;

    uint64_t end = info->addr + (uint64_t)info->height * info->pitch;
    g_ready = (info->type == 1 && (info->bpp == 32 || info->bpp == 24) &&
               info->addr != 0 && info->width > 0 && info->height > 0 &&
               end <= 0x100000000ULL && backbuffer != 0);
    return g_ready;
}

int fb_ready(void)  { return g_ready; }
int fb_width(void)  { return (int)g_fb.width; }
int fb_height(void) { return (int)g_fb.height; }

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (!g_ready || x >= g_fb.width || y >= g_fb.height)
        return;
    g_back[(uint64_t)y * g_fb.width + x] = color;
}

uint32_t fb_get_pixel(uint32_t x, uint32_t y)
{
    if (!g_ready || x >= g_fb.width || y >= g_fb.height)
        return 0;
    return g_back[(uint64_t)y * g_fb.width + x];
}

static void clamp_rect(int *x, int *y, int *w, int *h)
{
    if (*x < 0) { *w += *x; *x = 0; }
    if (*y < 0) { *h += *y; *y = 0; }
    if (*x + *w > (int)g_fb.width)  *w = (int)g_fb.width  - *x;
    if (*y + *h > (int)g_fb.height) *h = (int)g_fb.height - *y;
}

void fb_fill_rect(int x, int y, int w, int h, uint32_t color)
{
    if (!g_ready) return;
    clamp_rect(&x, &y, &w, &h);
    for (int j = 0; j < h; j++) {
        uint32_t *row = g_back + (uint64_t)(y + j) * g_fb.width + x;
        for (int i = 0; i < w; i++) row[i] = color;
    }
}

void fb_vgradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom)
{
    if (!g_ready || h <= 0) return;
    int tr = (top >> 16) & 0xFF, tg = (top >> 8) & 0xFF, tb = top & 0xFF;
    int br = (bottom >> 16) & 0xFF, bg = (bottom >> 8) & 0xFF, bb = bottom & 0xFF;
    for (int j = 0; j < h; j++) {
        int r = tr + (br - tr) * j / h;
        int g = tg + (bg - tg) * j / h;
        int b = tb + (bb - tb) * j / h;
        fb_fill_rect(x, y + j, w, 1, rgb((uint8_t)r, (uint8_t)g, (uint8_t)b));
    }
}

static int inside_round(int cx, int cy, int px, int py, int r)
{
    int dx = px - cx, dy = py - cy;
    return (dx * dx + dy * dy) <= (r * r);
}

void fb_rounded_rect(int x, int y, int w, int h, int radius, uint32_t color)
{
    if (!g_ready) return;
    if (radius * 2 > w) radius = w / 2;
    if (radius * 2 > h) radius = h / 2;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int px = x + i, py = y + j;
            if (i < radius && j < radius &&
                !inside_round(x + radius, y + radius, px, py, radius)) continue;
            if (i >= w - radius && j < radius &&
                !inside_round(x + w - radius - 1, y + radius, px, py, radius)) continue;
            if (i < radius && j >= h - radius &&
                !inside_round(x + radius, y + h - radius - 1, px, py, radius)) continue;
            if (i >= w - radius && j >= h - radius &&
                !inside_round(x + w - radius - 1, y + h - radius - 1, px, py, radius)) continue;
            fb_put_pixel((uint32_t)px, (uint32_t)py, color);
        }
    }
}

void fb_rect_border(int x, int y, int w, int h, int thick, uint32_t color)
{
    fb_fill_rect(x, y, w, thick, color);
    fb_fill_rect(x, y + h - thick, w, thick, color);
    fb_fill_rect(x, y, thick, h, color);
    fb_fill_rect(x + w - thick, y, thick, h, color);
}

void fb_draw_char(int x, int y, char c, uint32_t color, int scale)
{
    if (!g_ready) return;
    unsigned char uc = (unsigned char)c;
    if (uc >= 128) uc = '?';
    const uint8_t *glyph = font8x8_basic[uc];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++)
            if (bits & (1u << col))
                fb_fill_rect(x + col * scale, y + row * scale, scale, scale, color);
    }
}

void fb_draw_text(int x, int y, const char *s, uint32_t color, int scale)
{
    int cx = x;
    for (; *s; s++) {
        if (*s == '\n') { y += 8 * scale + scale; cx = x; continue; }
        fb_draw_char(cx, y, *s, color, scale);
        cx += 8 * scale;
    }
}

int fb_text_width(const char *s, int scale)
{
    int n = 0;
    for (; *s; s++) n++;
    return n * 8 * scale;
}

int fb_print(int x, int y, const char *s, uint32_t color, int scale)
{
    fb_draw_text(x, y, s, color, scale);
    return x + fb_text_width(s, scale);
}

int fb_print_uint(int x, int y, uint32_t v, uint32_t color, int scale)
{
    char buf[12]; int i = 0;
    if (v == 0) buf[i++] = '0';
    while (v > 0 && i < 11) { buf[i++] = (char)('0' + v % 10); v /= 10; }
    for (int a = 0, b = i - 1; a < b; a++, b--) { char t = buf[a]; buf[a] = buf[b]; buf[b] = t; }
    buf[i] = 0;
    return fb_print(x, y, buf, color, scale);
}

void fb_draw_cursor(int x, int y, uint32_t fill, uint32_t border)
{
    static const uint8_t arrow[12] = {
        0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0x0F, 0x1B, 0x19, 0x30, 0x30
    };
    for (int row = 0; row < 12; row++)
        for (int col = 0; col < 8; col++)
            if (arrow[row] & (1u << col)) {
                fb_fill_rect(x + col - 1, y + row, 1, 1, border);
                fb_fill_rect(x + col + 1, y + row, 1, 1, border);
                fb_fill_rect(x + col, y + row - 1, 1, 1, border);
            }
    for (int row = 0; row < 12; row++)
        for (int col = 0; col < 8; col++)
            if (arrow[row] & (1u << col))
                fb_put_pixel((uint32_t)(x + col), (uint32_t)(y + row), fill);
}

/* Blit a raw 32-bit BGRX image into the backbuffer at the top-left, clamped. */
void fb_blit_raw32(const void *src, uint32_t sw, uint32_t sh)
{
    if (!g_ready) return;
    uint32_t w = sw < g_fb.width  ? sw : g_fb.width;
    uint32_t h = sh < g_fb.height ? sh : g_fb.height;
    const uint32_t *s = (const uint32_t *)src;
    for (uint32_t y = 0; y < h; y++)
        memcpy(g_back + (uint64_t)y * g_fb.width, s + (uint64_t)y * sw, (size_t)w * 4);
}

/* Flush the backbuffer to the linear framebuffer. */
void fb_present(void)
{
    if (!g_ready) return;
    uint8_t *d = (uint8_t *)(uintptr_t)g_fb.addr;
    if (g_fb.bpp == 32 && g_fb.pitch == g_fb.width * 4) {
        memcpy(d, g_back, (size_t)g_fb.width * g_fb.height * 4);
    } else if (g_fb.bpp == 32) {
        for (uint32_t y = 0; y < g_fb.height; y++)
            memcpy(d + (uint64_t)y * g_fb.pitch,
                   g_back + (uint64_t)y * g_fb.width, (size_t)g_fb.width * 4);
    } else { /* 24 bpp */
        for (uint32_t y = 0; y < g_fb.height; y++) {
            uint8_t *row = d + (uint64_t)y * g_fb.pitch;
            const uint32_t *sb = g_back + (uint64_t)y * g_fb.width;
            for (uint32_t x = 0; x < g_fb.width; x++) {
                row[x*3+0] = (uint8_t)(sb[x] & 0xFF);
                row[x*3+1] = (uint8_t)((sb[x] >> 8) & 0xFF);
                row[x*3+2] = (uint8_t)((sb[x] >> 16) & 0xFF);
            }
        }
    }
    __asm__ volatile ("wbinvd" ::: "memory");
}
