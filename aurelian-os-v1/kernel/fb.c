/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * fb.c — double-buffered compositor
 *
 * Drawing goes to an off-screen backbuffer of 0x00RRGGBB pixels. The wallpaper
 * is upscaled once into a background cache (memcpy'd in at the start of each
 * frame), and a heavily downsampled+blurred copy backs the Mica material used
 * by windows, the taskbar and flyouts.
 * ==========================================================================*/

#include "fb.h"
#include "font8x16.h"
#include "string.h"

static struct fb_info g_fb;
static uint32_t      *g_back;   /* compositing target  */
static uint32_t      *g_bg;     /* upscaled wallpaper  */
static uint32_t      *g_fade;   /* previous wallpaper, for cross-fades */
static int            g_fade_t; /* 0..256, 256 = fade complete         */
static int            g_fading;
static uint8_t        g_ready;

/* Low-resolution blurred wallpaper: the Mica base. Sampling a small, blurred
 * image is equivalent to a wide-radius blur but costs almost nothing. */
#define BLUR_W 160
#define BLUR_H 120
static uint32_t g_blur[BLUR_W * BLUR_H];

/* screen coordinate -> blur texel maps, so Mica sampling needs no divides */
static uint16_t g_mx[FB_MAX_W];
static uint16_t g_my[FB_MAX_H];

/* ------------------------------------------------------------------ */
static uint32_t isqrt32(uint32_t v)
{
    uint32_t x = 0, bit = 1u << 30;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (x + bit <= v) { v -= x + bit; x = (x >> 1) + bit; }
        else               { x >>= 1; }
        bit >>= 2;
    }
    return x;
}

int fb_init(const struct fb_info *info, uint32_t *backbuffer, uint32_t *bgbuffer,
            uint32_t *fadebuffer)
{
    g_fb   = *info;
    g_back = backbuffer;
    g_bg   = bgbuffer;
    g_fade = fadebuffer;

    uint64_t end = info->addr + (uint64_t)info->height * info->pitch;
    g_ready = (info->type == 1 && (info->bpp == 32 || info->bpp == 24) &&
               info->addr != 0 && info->width > 0 && info->height > 0 &&
               info->width <= FB_MAX_W && info->height <= FB_MAX_H &&
               end <= 0x100000000ULL && backbuffer != 0 && bgbuffer != 0);
    if (g_ready) {
        for (uint32_t x = 0; x < g_fb.width; x++)
            g_mx[x] = (uint16_t)(x * BLUR_W / g_fb.width);
        for (uint32_t y = 0; y < g_fb.height; y++)
            g_my[y] = (uint16_t)(y * BLUR_H / g_fb.height);
    }
    return g_ready;
}

int fb_ready(void)  { return g_ready; }
int fb_width(void)  { return (int)g_fb.width; }
int fb_height(void) { return (int)g_fb.height; }

/* ------------------------------------------------------------------ */
/* Pixels and rectangles                                              */
/* ------------------------------------------------------------------ */
void fb_put_pixel(int x, int y, uint32_t color)
{
    if (!g_ready || x < 0 || y < 0 || x >= (int)g_fb.width || y >= (int)g_fb.height)
        return;
    g_back[(uint32_t)y * g_fb.width + (uint32_t)x] = color;
}

uint32_t fb_get_pixel(int x, int y)
{
    if (!g_ready || x < 0 || y < 0 || x >= (int)g_fb.width || y >= (int)g_fb.height)
        return 0;
    return g_back[(uint32_t)y * g_fb.width + (uint32_t)x];
}

static int clip(int *x, int *y, int *w, int *h)
{
    if (!g_ready) return 0;
    if (*x < 0) { *w += *x; *x = 0; }
    if (*y < 0) { *h += *y; *y = 0; }
    if (*x + *w > (int)g_fb.width)  *w = (int)g_fb.width  - *x;
    if (*y + *h > (int)g_fb.height) *h = (int)g_fb.height - *y;
    return *w > 0 && *h > 0;
}

void fb_fill(int x, int y, int w, int h, uint32_t color)
{
    if (!clip(&x, &y, &w, &h)) return;
    for (int j = 0; j < h; j++) {
        uint32_t *row = g_back + (uint32_t)(y + j) * g_fb.width + x;
        for (int i = 0; i < w; i++) row[i] = color;
    }
}

void fb_fill_a(int x, int y, int w, int h, uint32_t color, uint8_t alpha)
{
    if (alpha == 255) { fb_fill(x, y, w, h, color); return; }
    if (!clip(&x, &y, &w, &h)) return;
    for (int j = 0; j < h; j++) {
        uint32_t *row = g_back + (uint32_t)(y + j) * g_fb.width + x;
        for (int i = 0; i < w; i++) row[i] = blend(row[i], color, alpha);
    }
}

/* Horizontal inset of a rounded rect at row j (0..h-1). */
static int round_inset(int j, int h, int r)
{
    int dy;
    if (j < r)            dy = r - 1 - j;
    else if (j >= h - r)  dy = j - (h - r);
    else                  return 0;
    int rr = r * r - dy * dy;
    if (rr < 0) rr = 0;
    return r - (int)isqrt32((uint32_t)rr);
}

void fb_round(int x, int y, int w, int h, int r, uint32_t color)
{
    if (!g_ready || w <= 0 || h <= 0) return;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    for (int j = 0; j < h; j++) {
        int in = round_inset(j, h, r);
        fb_fill(x + in, y + j, w - 2 * in, 1, color);
    }
}

void fb_round_a(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha)
{
    if (!g_ready || w <= 0 || h <= 0) return;
    if (alpha == 255) { fb_round(x, y, w, h, r, color); return; }
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    for (int j = 0; j < h; j++) {
        int in = round_inset(j, h, r);
        fb_fill_a(x + in, y + j, w - 2 * in, 1, color, alpha);
    }
}

void fb_round_border(int x, int y, int w, int h, int r, int t, uint32_t color, uint8_t alpha)
{
    if (!g_ready || w <= 0 || h <= 0) return;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    for (int j = 0; j < h; j++) {
        int in = round_inset(j, h, r);
        if (j < t || j >= h - t) {
            fb_fill_a(x + in, y + j, w - 2 * in, 1, color, alpha);
        } else {
            /* left and right edges follow the same corner curve */
            fb_fill_a(x + in, y + j, t, 1, color, alpha);
            fb_fill_a(x + w - in - t, y + j, t, 1, color, alpha);
        }
    }
}

void fb_border(int x, int y, int w, int h, int t, uint32_t color)
{
    fb_fill(x, y, w, t, color);
    fb_fill(x, y + h - t, w, t, color);
    fb_fill(x, y, t, h, color);
    fb_fill(x + w - t, y, t, h, color);
}

void fb_vgrad(int x, int y, int w, int h, uint32_t top, uint32_t bottom)
{
    if (h <= 0) return;
    int tr = (top >> 16) & 0xFF, tg = (top >> 8) & 0xFF, tb = top & 0xFF;
    int br = (bottom >> 16) & 0xFF, bg = (bottom >> 8) & 0xFF, bb = bottom & 0xFF;
    for (int j = 0; j < h; j++) {
        int r = tr + (br - tr) * j / h;
        int g = tg + (bg - tg) * j / h;
        int b = tb + (bb - tb) * j / h;
        fb_fill(x, y + j, w, 1, rgb((uint8_t)r, (uint8_t)g, (uint8_t)b));
    }
}

/* Soft shadow: concentric 1px rings at increasing alpha. Only the ring is
 * drawn (the window covers the interior), so cost is O(perimeter * spread)
 * rather than O(area * spread). */
void fb_shadow(int x, int y, int w, int h, int r, int spread)
{
    for (int i = spread; i >= 1; i--) {
        uint8_t a = (uint8_t)(6 + (spread - i) * 5);
        fb_round_border(x - i, y - i + spread / 3, w + 2 * i, h + 2 * i,
                        r + i, 1, 0x00000000u, a);
    }
}

/* ------------------------------------------------------------------ */
/* Text (8x16, MSB-first glyph rows)                                  */
/* ------------------------------------------------------------------ */
void fb_glyph(int x, int y, char c, uint32_t color, int scale)
{
    unsigned char uc = (unsigned char)c;
    if (uc < 0x20 || uc > 0x7E) uc = '?';
    const uint8_t *g = font8x16[uc - 0x20];
    if (scale == 1) {
        for (int row = 0; row < FONT_H; row++) {
            uint8_t bits = g[row];
            if (!bits) continue;
            for (int col = 0; col < FONT_W; col++)
                if (bits & (0x80u >> col))
                    fb_put_pixel(x + col, y + row, color);
        }
    } else {
        for (int row = 0; row < FONT_H; row++) {
            uint8_t bits = g[row];
            if (!bits) continue;
            for (int col = 0; col < FONT_W; col++)
                if (bits & (0x80u >> col))
                    fb_fill(x + col * scale, y + row * scale, scale, scale, color);
        }
    }
}

int fb_text(int x, int y, const char *s, uint32_t color, int scale)
{
    int cx = x;
    for (; *s; s++) {
        if (*s == '\n') { y += FONT_H * scale; cx = x; continue; }
        fb_glyph(cx, y, *s, color, scale);
        cx += FONT_W * scale;
    }
    return cx;
}

int fb_text_w(const char *s, int scale)
{
    int n = 0;
    for (; *s; s++) n++;
    return n * FONT_W * scale;
}

void fb_text_c(int x, int y, int w, const char *s, uint32_t color, int scale)
{
    fb_text(x + (w - fb_text_w(s, scale)) / 2, y, s, color, scale);
}

int fb_num(int x, int y, uint32_t v, uint32_t color, int scale)
{
    char b[12]; int i = 0;
    if (v == 0) b[i++] = '0';
    while (v && i < 11) { b[i++] = (char)('0' + v % 10); v /= 10; }
    for (int a = 0, z = i - 1; a < z; a++, z--) { char t = b[a]; b[a] = b[z]; b[z] = t; }
    b[i] = 0;
    return fb_text(x, y, b, color, scale);
}

int fb_num2(int x, int y, uint32_t v, uint32_t color, int scale)
{
    char b[3];
    b[0] = (char)('0' + (v / 10) % 10);
    b[1] = (char)('0' + v % 10);
    b[2] = 0;
    return fb_text(x, y, b, color, scale);
}

/* ------------------------------------------------------------------ */
/* Wallpaper / background / Mica                                      */
/* ------------------------------------------------------------------ */

/* Build the low-res blurred Mica base from the full-res background cache. */
static void build_blur(void)
{
    uint32_t W = g_fb.width, H = g_fb.height;
    /* box-average the background down to BLUR_W x BLUR_H */
    for (int by = 0; by < BLUR_H; by++) {
        uint32_t y0 = (uint32_t)by * H / BLUR_H;
        uint32_t y1 = (uint32_t)(by + 1) * H / BLUR_H;
        if (y1 <= y0) y1 = y0 + 1;
        for (int bx = 0; bx < BLUR_W; bx++) {
            uint32_t x0 = (uint32_t)bx * W / BLUR_W;
            uint32_t x1 = (uint32_t)(bx + 1) * W / BLUR_W;
            if (x1 <= x0) x1 = x0 + 1;
            uint32_t r = 0, g = 0, b = 0, n = 0;
            for (uint32_t y = y0; y < y1; y += 2) {
                const uint32_t *row = g_bg + (uint64_t)y * W;
                for (uint32_t x = x0; x < x1; x += 2) {
                    uint32_t p = row[x];
                    r += (p >> 16) & 0xFF; g += (p >> 8) & 0xFF; b += p & 0xFF;
                    n++;
                }
            }
            if (!n) n = 1;
            g_blur[by * BLUR_W + bx] = ((r / n) << 16) | ((g / n) << 8) | (b / n);
        }
    }
    /* two 3x3 box passes to smooth it out */
    for (int pass = 0; pass < 2; pass++) {
        for (int y = 1; y < BLUR_H - 1; y++) {
            for (int x = 1; x < BLUR_W - 1; x++) {
                uint32_t r = 0, g = 0, b = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        uint32_t p = g_blur[(y + dy) * BLUR_W + (x + dx)];
                        r += (p >> 16) & 0xFF; g += (p >> 8) & 0xFF; b += p & 0xFF;
                    }
                g_blur[y * BLUR_W + x] = ((r / 9) << 16) | ((g / 9) << 8) | (b / 9);
            }
        }
    }
}

void fb_set_wallpaper(const void *src, uint32_t sw, uint32_t sh)
{
    if (!g_ready || sw == 0 || sh == 0) return;
    uint32_t W = g_fb.width, H = g_fb.height;
    const uint32_t *s = (const uint32_t *)src;
    /* nearest-neighbour upscale (sources are pre-cropped to the screen ratio) */
    for (uint32_t y = 0; y < H; y++) {
        const uint32_t *srow = s + (uint64_t)(y * sh / H) * sw;
        uint32_t *drow = g_bg + (uint64_t)y * W;
        for (uint32_t x = 0; x < W; x++)
            drow[x] = srow[x * sw / W] & 0x00FFFFFFu;
    }
    build_blur();
}

void fb_set_wallpaper_gradient(uint32_t top, uint32_t bottom)
{
    if (!g_ready) return;
    uint32_t W = g_fb.width, H = g_fb.height;
    int tr = (top >> 16) & 0xFF, tg = (top >> 8) & 0xFF, tb = top & 0xFF;
    int br = (bottom >> 16) & 0xFF, bg = (bottom >> 8) & 0xFF, bb = bottom & 0xFF;
    for (uint32_t y = 0; y < H; y++) {
        int r = tr + (br - tr) * (int)y / (int)H;
        int g = tg + (bg - tg) * (int)y / (int)H;
        int b = tb + (bb - tb) * (int)y / (int)H;
        uint32_t c = rgb((uint8_t)r, (uint8_t)g, (uint8_t)b);
        uint32_t *drow = g_bg + (uint64_t)y * W;
        for (uint32_t x = 0; x < W; x++) drow[x] = c;
    }
    build_blur();
}

void fb_fade_begin(void)
{
    if (!g_ready || !g_fade) return;
    memcpy(g_fade, g_bg, (size_t)g_fb.width * g_fb.height * 4);
    g_fade_t = 0;
    g_fading = 1;
}

int fb_fade_step(int delta)
{
    if (!g_fading) return 0;
    g_fade_t += delta;
    if (g_fade_t >= 256) { g_fade_t = 256; g_fading = 0; }
    return g_fading;
}

int fb_fade_active(void) { return g_fading; }

void fb_draw_background(void)
{
    if (!g_ready) return;
    uint32_t n = g_fb.width * g_fb.height;
    if (!g_fading) { memcpy(g_back, g_bg, (size_t)n * 4); return; }
    uint8_t a = (uint8_t)(g_fade_t > 255 ? 255 : g_fade_t);
    for (uint32_t i = 0; i < n; i++)
        g_back[i] = blend(g_fade[i], g_bg[i], a);
}

uint32_t fb_mica_at(int x, int y)
{
    if (!g_ready) return 0;
    if (x < 0) x = 0;
    if (x >= (int)g_fb.width)  x = (int)g_fb.width  - 1;
    if (y < 0) y = 0;
    if (y >= (int)g_fb.height) y = (int)g_fb.height - 1;
    return g_blur[(uint32_t)g_my[y] * BLUR_W + g_mx[x]];
}

void fb_mica_round(int x, int y, int w, int h, int r, uint32_t tint, uint8_t alpha)
{
    if (!g_ready || w <= 0 || h <= 0) return;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    for (int j = 0; j < h; j++) {
        int in = round_inset(j, h, r);
        int rx = x + in, rw = w - 2 * in, ry = y + j;
        if (ry < 0 || ry >= (int)g_fb.height) continue;
        if (rx < 0) { rw += rx; rx = 0; }
        if (rx + rw > (int)g_fb.width) rw = (int)g_fb.width - rx;
        if (rw <= 0) continue;
        const uint32_t *brow = g_blur + (uint32_t)g_my[ry] * BLUR_W;
        uint32_t *row = g_back + (uint32_t)ry * g_fb.width + rx;
        for (int i = 0; i < rw; i++)
            row[i] = blend(brow[g_mx[rx + i]], tint, alpha);
    }
}

/* ------------------------------------------------------------------ */
/* Cursor                                                             */
/* ------------------------------------------------------------------ */
void fb_cursor(int x, int y)
{
    /* 12x19 arrow: 1 = white fill, 2 = dark outline */
    static const char *art[19] = {
        "2...........",
        "22..........",
        "212.........",
        "2112........",
        "21112.......",
        "211112......",
        "2111112.....",
        "21111112....",
        "211111112...",
        "2111111112..",
        "21111111112.",
        "211111122222",
        "21112112....",
        "2112.2112...",
        "212..2112...",
        "22....2112..",
        "2.....2112..",
        ".......211..",
        ".......22..."
    };
    for (int j = 0; j < 19; j++)
        for (int i = 0; i < 12; i++) {
            char c = art[j][i];
            if (c == '1')      fb_put_pixel(x + i, y + j, 0x00FFFFFFu);
            else if (c == '2') fb_put_pixel(x + i, y + j, 0x00202020u);
        }
}

/* ------------------------------------------------------------------ */
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
