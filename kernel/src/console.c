#include <aurelian/kernel.h>

enum {
    VGA_WIDTH = 80,
    VGA_HEIGHT = 25,
    VGA_DEFAULT_COLOR = 0x0f,
};

static uint16_t vga_entry(char c, uint8_t color)
{
    return (uint16_t)c | ((uint16_t)color << 8);
}

static void console_newline(struct aurelion_console *console)
{
    console->column = 0;
    if (console->row + 1 < VGA_HEIGHT) {
        console->row++;
    }
}

void aurelion_console_init(struct aurelion_console *console, volatile uint16_t *vga_text)
{
    console->vga_text = vga_text;
    console->column = 0;
    console->row = 0;
    console->color = VGA_DEFAULT_COLOR;

    for (size_t row = 0; row < VGA_HEIGHT; row++) {
        for (size_t column = 0; column < VGA_WIDTH; column++) {
            console->vga_text[(row * VGA_WIDTH) + column] = vga_entry(' ', console->color);
        }
    }
}

void aurelion_console_write(struct aurelion_console *console, const char *text)
{
    for (size_t i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            console_newline(console);
            continue;
        }

        console->vga_text[(console->row * VGA_WIDTH) + console->column] = vga_entry(text[i], console->color);
        console->column++;

        if (console->column == VGA_WIDTH) {
            console_newline(console);
        }
    }
}
