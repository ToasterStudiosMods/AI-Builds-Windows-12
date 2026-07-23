#include "aurelian/framebuffer.h"

#define AURELION_FRAMEBUFFER_MAX_DIMENSION 8192U

struct aurelion_rgb {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

static uint32_t min_u32(uint32_t left, uint32_t right)
{
    return left < right ? left : right;
}

static uint32_t max_u32(uint32_t left, uint32_t right)
{
    return left > right ? left : right;
}

static int32_t abs_i32(int32_t value)
{
    return value < 0 ? -value : value;
}

static uint32_t scale_component(uint8_t value, uint8_t size)
{
    uint32_t max_value;

    if (size == 0 || size > 8) {
        return 0;
    }

    max_value = (1U << size) - 1U;
    return ((uint32_t)value * max_value + 127U) / 255U;
}

static uint32_t pack_rgb(const struct aurelion_framebuffer_surface *surface,
                         struct aurelion_rgb color)
{
    return (scale_component(color.red, surface->red_mask_size)
            << surface->red_mask_shift) |
           (scale_component(color.green, surface->green_mask_size)
            << surface->green_mask_shift) |
           (scale_component(color.blue, surface->blue_mask_size)
            << surface->blue_mask_shift);
}

static void put_pixel(struct aurelion_framebuffer_surface *surface,
                      uint32_t x, uint32_t y, struct aurelion_rgb color)
{
    volatile uint8_t *pixel;
    uint32_t packed;
    uint32_t byte;

    if (x >= surface->width || y >= surface->height) {
        return;
    }

    pixel = surface->pixels + ((uint64_t)y * surface->pitch) +
            ((uint64_t)x * surface->bytes_per_pixel);
    packed = pack_rgb(surface, color);
    for (byte = 0; byte < surface->bytes_per_pixel; byte++) {
        pixel[byte] = (uint8_t)(packed >> (byte * 8U));
    }
}

static void fill_rect(struct aurelion_framebuffer_surface *surface,
                      uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                      struct aurelion_rgb color)
{
    uint32_t last_x = min_u32(surface->width, x + width);
    uint32_t last_y = min_u32(surface->height, y + height);

    for (uint32_t py = y; py < last_y; py++) {
        for (uint32_t px = x; px < last_x; px++) {
            put_pixel(surface, px, py, color);
        }
    }
}

static int point_in_rounded_rect(uint32_t x, uint32_t y, uint32_t width,
                                 uint32_t height, uint32_t radius)
{
    int32_t center_x;
    int32_t center_y;
    int32_t dx;
    int32_t dy;

    if (radius == 0 || (x >= radius && x < width - radius) ||
        (y >= radius && y < height - radius)) {
        return 1;
    }

    center_x = x < radius ? (int32_t)radius : (int32_t)(width - radius - 1U);
    center_y = y < radius ? (int32_t)radius : (int32_t)(height - radius - 1U);
    dx = (int32_t)x - center_x;
    dy = (int32_t)y - center_y;
    return (uint32_t)(dx * dx + dy * dy) <= radius * radius;
}

static void fill_rounded_rect(struct aurelion_framebuffer_surface *surface,
                              uint32_t x, uint32_t y, uint32_t width,
                              uint32_t height, uint32_t radius,
                              struct aurelion_rgb color)
{
    uint32_t clipped_width;
    uint32_t clipped_height;

    if (x >= surface->width || y >= surface->height) {
        return;
    }

    clipped_width = min_u32(width, surface->width - x);
    clipped_height = min_u32(height, surface->height - y);
    radius = min_u32(radius, min_u32(clipped_width / 2U, clipped_height / 2U));

    for (uint32_t py = 0; py < clipped_height; py++) {
        for (uint32_t px = 0; px < clipped_width; px++) {
            if (point_in_rounded_rect(px, py, clipped_width, clipped_height,
                                      radius)) {
                put_pixel(surface, x + px, y + py, color);
            }
        }
    }
}

static void fill_disc(struct aurelion_framebuffer_surface *surface,
                      int32_t center_x, int32_t center_y, uint32_t radius,
                      struct aurelion_rgb color)
{
    int32_t start_x = center_x > (int32_t)radius
                          ? center_x - (int32_t)radius : 0;
    int32_t start_y = center_y > (int32_t)radius
                          ? center_y - (int32_t)radius : 0;
    int32_t end_x = min_u32(surface->width,
                            (uint32_t)(center_x + (int32_t)radius + 1));
    int32_t end_y = min_u32(surface->height,
                            (uint32_t)(center_y + (int32_t)radius + 1));
    int32_t squared_radius = (int32_t)(radius * radius);

    for (int32_t py = start_y; py < end_y; py++) {
        for (int32_t px = start_x; px < end_x; px++) {
            int32_t dx = px - center_x;
            int32_t dy = py - center_y;
            if (dx * dx + dy * dy <= squared_radius) {
                put_pixel(surface, (uint32_t)px, (uint32_t)py, color);
            }
        }
    }
}

static void draw_thick_line(struct aurelion_framebuffer_surface *surface,
                            int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                            uint32_t thickness, struct aurelion_rgb color)
{
    int32_t dx = abs_i32(x1 - x0);
    int32_t sx = x0 < x1 ? 1 : -1;
    int32_t dy = -abs_i32(y1 - y0);
    int32_t sy = y0 < y1 ? 1 : -1;
    int32_t error = dx + dy;

    for (;;) {
        fill_disc(surface, x0, y0, thickness / 2U, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        {
            int32_t twice_error = 2 * error;
            if (twice_error >= dy) {
                error += dy;
                x0 += sx;
            }
            if (twice_error <= dx) {
                error += dx;
                y0 += sy;
            }
        }
    }
}

/* A compact 5x7 all-caps font keeps the early boot path asset-free. */
static const uint8_t glyph_space[7] = { 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t glyph_period[7] = { 0, 0, 0, 0, 0, 0x06, 0x06 };
static const uint8_t glyph_dash[7] = { 0, 0, 0, 0x1f, 0, 0, 0 };
static const uint8_t glyph_zero[7] = { 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e };
static const uint8_t glyph_one[7] = { 0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e };
static const uint8_t glyph_two[7] = { 0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f };
static const uint8_t glyph_three[7] = { 0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e };
static const uint8_t glyph_four[7] = { 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02 };
static const uint8_t glyph_five[7] = { 0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e };
static const uint8_t glyph_six[7] = { 0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e };
static const uint8_t glyph_seven[7] = { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 };
static const uint8_t glyph_eight[7] = { 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e };
static const uint8_t glyph_nine[7] = { 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c };
static const uint8_t glyph_a[7] = { 0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 };
static const uint8_t glyph_b[7] = { 0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e };
static const uint8_t glyph_c[7] = { 0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e };
static const uint8_t glyph_d[7] = { 0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e };
static const uint8_t glyph_e[7] = { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f };
static const uint8_t glyph_f[7] = { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10 };
static const uint8_t glyph_g[7] = { 0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0e };
static const uint8_t glyph_h[7] = { 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 };
static const uint8_t glyph_i[7] = { 0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e };
static const uint8_t glyph_j[7] = { 0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0c };
static const uint8_t glyph_k[7] = { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 };
static const uint8_t glyph_l[7] = { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f };
static const uint8_t glyph_m[7] = { 0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11 };
static const uint8_t glyph_n[7] = { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 };
static const uint8_t glyph_o[7] = { 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e };
static const uint8_t glyph_p[7] = { 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10 };
static const uint8_t glyph_q[7] = { 0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d };
static const uint8_t glyph_r[7] = { 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11 };
static const uint8_t glyph_s[7] = { 0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e };
static const uint8_t glyph_t[7] = { 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 };
static const uint8_t glyph_u[7] = { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e };
static const uint8_t glyph_v[7] = { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04 };
static const uint8_t glyph_w[7] = { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a };
static const uint8_t glyph_x[7] = { 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11 };
static const uint8_t glyph_y[7] = { 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04 };
static const uint8_t glyph_z[7] = { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f };

static const uint8_t *glyph_for(char character)
{
    switch (character) {
    case '0': return glyph_zero;
    case '1': return glyph_one;
    case '2': return glyph_two;
    case '3': return glyph_three;
    case '4': return glyph_four;
    case '5': return glyph_five;
    case '6': return glyph_six;
    case '7': return glyph_seven;
    case '8': return glyph_eight;
    case '9': return glyph_nine;
    case 'A': return glyph_a;
    case 'B': return glyph_b;
    case 'C': return glyph_c;
    case 'D': return glyph_d;
    case 'E': return glyph_e;
    case 'F': return glyph_f;
    case 'G': return glyph_g;
    case 'H': return glyph_h;
    case 'I': return glyph_i;
    case 'J': return glyph_j;
    case 'K': return glyph_k;
    case 'L': return glyph_l;
    case 'M': return glyph_m;
    case 'N': return glyph_n;
    case 'O': return glyph_o;
    case 'P': return glyph_p;
    case 'Q': return glyph_q;
    case 'R': return glyph_r;
    case 'S': return glyph_s;
    case 'T': return glyph_t;
    case 'U': return glyph_u;
    case 'V': return glyph_v;
    case 'W': return glyph_w;
    case 'X': return glyph_x;
    case 'Y': return glyph_y;
    case 'Z': return glyph_z;
    case '.': return glyph_period;
    case '-': return glyph_dash;
    case ' ': return glyph_space;
    default: return glyph_space;
    }
}

static uint32_t text_width(const char *text, uint32_t scale)
{
    uint32_t width = 0;
    for (uint32_t index = 0; text[index] != '\0'; index++) {
        width += 6U * scale;
    }
    return width == 0 ? 0 : width - scale;
}

static void draw_text(struct aurelion_framebuffer_surface *surface,
                      uint32_t x, uint32_t y, const char *text, uint32_t scale,
                      struct aurelion_rgb color)
{
    for (uint32_t index = 0; text[index] != '\0'; index++) {
        const uint8_t *glyph = glyph_for(text[index]);
        for (uint32_t row = 0; row < 7; row++) {
            for (uint32_t column = 0; column < 5; column++) {
                if (glyph[row] & (1U << (4U - column))) {
                    fill_rect(surface, x + (index * 6U + column) * scale,
                              y + row * scale, scale, scale, color);
                }
            }
        }
    }
}

static void draw_centered_text(struct aurelion_framebuffer_surface *surface,
                               uint32_t center_x, uint32_t y, const char *text,
                               uint32_t scale, struct aurelion_rgb color)
{
    uint32_t width = text_width(text, scale);
    uint32_t x = center_x > width / 2U ? center_x - width / 2U : 0;
    draw_text(surface, x, y, text, scale, color);
}

int aurelion_framebuffer_init(struct aurelion_framebuffer_surface *surface,
                              const struct aurelion_framebuffer *framebuffer)
{
    uint64_t minimum_pitch;

    if (surface == 0 || framebuffer == 0 || framebuffer->address == 0 ||
        framebuffer->width == 0 || framebuffer->height == 0 ||
        framebuffer->width > AURELION_FRAMEBUFFER_MAX_DIMENSION ||
        framebuffer->height > AURELION_FRAMEBUFFER_MAX_DIMENSION ||
        (framebuffer->bpp != 24 && framebuffer->bpp != 32)) {
        return 0;
    }

    minimum_pitch = (uint64_t)framebuffer->width * (framebuffer->bpp / 8U);
    if (framebuffer->pitch < minimum_pitch) {
        return 0;
    }

    surface->pixels = (volatile uint8_t *)(uintptr_t)framebuffer->address;
    surface->width = framebuffer->width;
    surface->height = framebuffer->height;
    surface->pitch = framebuffer->pitch;
    surface->bytes_per_pixel = (uint8_t)(framebuffer->bpp / 8U);
    surface->red_mask_size = framebuffer->red_mask_size;
    surface->red_mask_shift = framebuffer->red_mask_shift;
    surface->green_mask_size = framebuffer->green_mask_size;
    surface->green_mask_shift = framebuffer->green_mask_shift;
    surface->blue_mask_size = framebuffer->blue_mask_size;
    surface->blue_mask_shift = framebuffer->blue_mask_shift;

    if (surface->red_mask_size == 0 || surface->green_mask_size == 0 ||
        surface->blue_mask_size == 0 || surface->red_mask_size > 8 ||
        surface->green_mask_size > 8 || surface->blue_mask_size > 8 ||
        surface->red_mask_shift > 31 || surface->green_mask_shift > 31 ||
        surface->blue_mask_shift > 31 ||
        (uint32_t)surface->red_mask_size + surface->red_mask_shift > 32U ||
        (uint32_t)surface->green_mask_size + surface->green_mask_shift > 32U ||
        (uint32_t)surface->blue_mask_size + surface->blue_mask_shift > 32U) {
        return 0;
    }

    return 1;
}

void aurelion_framebuffer_rebase(struct aurelion_framebuffer_surface *surface,
                                 void *virtual_address)
{
    if (surface != 0 && virtual_address != 0) {
        surface->pixels = (volatile uint8_t *)virtual_address;
    }
}

void aurelion_framebuffer_draw_boot_splash(
    struct aurelion_framebuffer_surface *surface, uint32_t progress)
{
    const struct aurelion_rgb navy = { 11, 18, 43 };
    const struct aurelion_rgb card_shadow = { 7, 11, 29 };
    const struct aurelion_rgb card = { 25, 36, 75 };
    const struct aurelion_rgb cyan = { 105, 226, 255 };
    const struct aurelion_rgb violet = { 171, 139, 255 };
    const struct aurelion_rgb white = { 244, 248, 255 };
    const struct aurelion_rgb muted = { 168, 184, 220 };
    const char *caption;
    uint32_t card_width;
    uint32_t card_height;
    uint32_t card_x;
    uint32_t card_y;
    uint32_t center_x;
    uint32_t logo_y;
    uint32_t rail_width;
    uint32_t rail_y;

    if (surface == 0 || surface->pixels == 0) {
        return;
    }

    progress = min_u32(progress, 4U);
    for (uint32_t y = 0; y < surface->height; y++) {
        for (uint32_t x = 0; x < surface->width; x++) {
            uint32_t horizontal = x * 30U / max_u32(surface->width, 1U);
            uint32_t vertical = y * 24U / max_u32(surface->height, 1U);
            uint32_t right_glow = x > surface->width / 2U
                                      ? (x - surface->width / 2U) * 19U /
                                            max_u32(surface->width, 1U)
                                      : 0;
            struct aurelion_rgb background = {
                (uint8_t)(navy.red + vertical / 3U),
                (uint8_t)(navy.green + horizontal / 2U + vertical / 4U),
                (uint8_t)(navy.blue + horizontal + right_glow)
            };
            put_pixel(surface, x, y, background);
        }
    }

    card_width = min_u32(surface->width > 48U ? surface->width - 48U : surface->width,
                         720U);
    card_height = min_u32(surface->height > 40U ? surface->height - 40U : surface->height,
                          336U);
    if (card_width < 240U || card_height < 180U) {
        return;
    }

    card_x = (surface->width - card_width) / 2U;
    card_y = (surface->height - card_height) / 2U;
    center_x = card_x + card_width / 2U;
    logo_y = card_y + card_height / 4U;

    fill_disc(surface, (int32_t)card_x + 72, (int32_t)card_y + 54, 46,
              (struct aurelion_rgb){ 32, 83, 142 });
    fill_disc(surface, (int32_t)(card_x + card_width - 80U),
              (int32_t)(card_y + card_height - 52U), 58,
              (struct aurelion_rgb){ 70, 48, 129 });
    fill_rounded_rect(surface, card_x + 8U, card_y + 12U, card_width,
                      card_height, 28U, card_shadow);
    fill_rounded_rect(surface, card_x, card_y, card_width, card_height, 28U,
                      card);
    fill_rounded_rect(surface, card_x + 1U, card_y + 1U, card_width - 2U,
                      2U, 1U, (struct aurelion_rgb){ 121, 150, 221 });

    draw_thick_line(surface, (int32_t)center_x - 42, (int32_t)logo_y + 38,
                    (int32_t)center_x, (int32_t)logo_y - 42, 12U, cyan);
    draw_thick_line(surface, (int32_t)center_x, (int32_t)logo_y - 42,
                    (int32_t)center_x + 42, (int32_t)logo_y + 38, 12U, violet);
    draw_thick_line(surface, (int32_t)center_x - 23, (int32_t)logo_y + 8,
                    (int32_t)center_x + 23, (int32_t)logo_y + 8, 9U, white);

    draw_centered_text(surface, center_x, card_y + card_height / 2U - 20U,
                       "AURELIAN", 5U, white);
    draw_centered_text(surface, center_x, card_y + card_height / 2U + 24U,
                       "PRISM BOOT CANVAS", 2U, cyan);

    rail_width = (card_width - 96U) / 4U;
    rail_y = card_y + card_height - 82U;
    for (uint32_t rail = 0; rail < 4U; rail++) {
        struct aurelion_rgb rail_color = rail < progress ? cyan :
            (struct aurelion_rgb){ 70, 87, 130 };
        fill_rounded_rect(surface, card_x + 36U + rail * (rail_width + 8U),
                          rail_y, rail_width, 7U, 3U, rail_color);
    }

    if (progress == 0U) {
        caption = "PREPARING PRISM UI";
    } else if (progress == 1U) {
        caption = "VALIDATING BOOT ABI";
    } else if (progress < 4U) {
        caption = "STARTING PLATFORM SERVICES";
    } else {
        caption = "BOOT SEQUENCE COMPLETE";
    }
    draw_centered_text(surface, center_x, rail_y + 26U, caption, 2U, muted);
    draw_centered_text(surface, center_x, card_y + card_height - 28U,
                       "AURELION 0.0.1", 1U, muted);
}
