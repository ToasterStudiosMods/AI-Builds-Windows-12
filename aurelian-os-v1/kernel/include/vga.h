/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * include/vga.h — VGA color text-mode driver (80x25)
 * ==========================================================================*/

#ifndef AURELIAN_VGA_H
#define AURELIAN_VGA_H

#include <stdint.h>

/* Hardware text mode is 80 columns x 25 rows. */
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

/* VGA memory-mapped I/O buffer (physical; identity-mapped in v1). */
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)

/* Standard VGA 16-color palette. */
enum vga_color {
    VGA_BLACK         = 0,
    VGA_BLUE          = 1,
    VGA_GREEN         = 2,
    VGA_CYAN          = 3,
    VGA_RED           = 4,
    VGA_MAGENTA       = 5,
    VGA_BROWN         = 6,
    VGA_LIGHT_GREY    = 7,
    VGA_DARK_GREY     = 8,
    VGA_LIGHT_BLUE    = 9,
    VGA_LIGHT_GREEN   = 10,
    VGA_LIGHT_CYAN    = 11,
    VGA_LIGHT_RED     = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW        = 14,
    VGA_WHITE         = 15,
};

/* Build a VGA color attribute byte from foreground + background. */
static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg)
{
    return (uint8_t)(fg | (bg << 4));
}

/* Build a full VGA character cell (char + attribute). */
static inline uint16_t vga_entry(unsigned char uc, uint8_t color)
{
    return (uint16_t)(uc | ((uint16_t)color << 8));
}

void vga_init(void);
void vga_set_color(uint8_t color);
void vga_clear(void);
void vga_putc(char c);
void vga_puts(const char *str);
void vga_newline(void);

#endif /* AURELIAN_VGA_H */
