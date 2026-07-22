#ifndef AURELIAN_KERNEL_H
#define AURELIAN_KERNEL_H

#include <stddef.h>
#include <stdint.h>
#include <aurelian/boot.h>

#define AURELION_KERNEL_NAME "Aurelion"
#define AURELION_KERNEL_VERSION "0.0.1"

struct aurelion_console {
    volatile uint16_t *vga_text;
    size_t column;
    size_t row;
    uint8_t color;
};

void aurelion_console_init(struct aurelion_console *console, volatile uint16_t *vga_text);
void aurelion_console_write(struct aurelion_console *console, const char *text);
void aurelion_kernel_main(const struct aurelion_boot_info *boot_info);
int aurelion_validate_boot_info(const struct aurelion_boot_info *boot_info);

#endif
