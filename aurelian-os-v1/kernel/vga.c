/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * vga.c — VGA color text-mode driver (80x25)
 * ==========================================================================*/

#include "vga.h"
#include "string.h"

static size_t    vga_row;
static size_t    vga_col;
static uint8_t   vga_color;
static uint16_t *vga_buf = (uint16_t *)VGA_MEMORY;

void vga_init(void)
{
    vga_row   = 0;
    vga_col   = 0;
    vga_color = vga_entry_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
}

void vga_set_color(uint8_t color)
{
    vga_color = color;
}

void vga_clear(void)
{
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga_buf[i] = vga_entry(' ', vga_color);
    vga_row = 0;
    vga_col = 0;
}

void vga_newline(void)
{
    vga_col = 0;
    if (++vga_row >= VGA_HEIGHT) {
        /* Scroll up one line. */
        for (size_t y = 1; y < VGA_HEIGHT; y++)
            for (size_t x = 0; x < VGA_WIDTH; x++)
                vga_buf[(y - 1) * VGA_WIDTH + x] =
                    vga_buf[y * VGA_WIDTH + x];
        /* Clear the last line. */
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga_buf[(VGA_HEIGHT - 1) * VGA_WIDTH + x] =
                vga_entry(' ', vga_color);
        vga_row = VGA_HEIGHT - 1;
    }
}

void vga_putc(char c)
{
    if (c == '\n') {
        vga_newline();
        return;
    }
    if (c == '\r') {
        vga_col = 0;
        return;
    }
    if (c == '\t') {
        vga_col = (vga_col + 8) & ~((size_t)7);
        if (vga_col >= VGA_WIDTH) vga_newline();
        return;
    }
    vga_buf[vga_row * VGA_WIDTH + vga_col] = vga_entry((unsigned char)c, vga_color);
    if (++vga_col >= VGA_WIDTH) vga_newline();
}

void vga_puts(const char *str)
{
    for (size_t i = 0; str[i]; i++)
        vga_putc(str[i]);
}
