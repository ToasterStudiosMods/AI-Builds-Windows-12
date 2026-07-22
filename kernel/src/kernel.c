#include <aurelian/kernel.h>

#define VGA_TEXT_BUFFER ((volatile uint16_t *)0xB8000)

int aurelion_validate_boot_info(const struct aurelion_boot_info *boot_info)
{
    if (boot_info == 0) {
        return 0;
    }

    if (boot_info->magic != AURELION_BOOT_MAGIC) {
        return 0;
    }

    if (boot_info->abi_version != AURELION_BOOT_ABI_VERSION) {
        return 0;
    }

    if (boot_info->struct_size < sizeof(struct aurelion_boot_info)) {
        return 0;
    }

    return 1;
}

void aurelion_kernel_main(const struct aurelion_boot_info *boot_info)
{
    struct aurelion_console console;
    aurelion_console_init(&console, VGA_TEXT_BUFFER);

    aurelion_console_write(&console, "Aurelion kernel 0.0.1\n");

    if (!aurelion_validate_boot_info(boot_info)) {
        aurelion_console_write(&console, "Boot ABI validation failed. Halting.\n");
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    aurelion_console_write(&console, "Boot ABI validated.\n");
    aurelion_console_write(&console, "Next: memory map, GDT/IDT, paging, scheduler.\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
