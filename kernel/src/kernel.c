#include "aurelian/kernel.h"
#include "aurelian/serial.h"
#include "aurelian/gdt.h"
#include "aurelian/idt.h"
#include "aurelian/pmm.h"
#include "aurelian/vmm.h"
#include "aurelian/heap.h"
#include "aurelian/framebuffer.h"

#define VGA_TEXT_BUFFER ((volatile uint16_t *)0xB8000)

/* Helper: print a 64-bit unsigned integer via serial */
static void serial_print_u64(uint64_t val)
{
    char buf[21];
    int i = 0;
    if (val == 0) {
        aurelion_serial_putc('0');
        return;
    }
    while (val > 0 && i < 20) {
        buf[i++] = '0' + (char)(val % 10);
        val /= 10;
    }
    for (int j = i - 1; j >= 0; j--) {
        aurelion_serial_putc(buf[j]);
    }
}

static void print_serial(const struct aurelion_console *console, const char *text)
{
    aurelion_serial_write(text);
    if (console) {
        aurelion_console_write((struct aurelion_console *)console, text);
    }
}

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

    if (boot_info->struct_size != sizeof(struct aurelion_boot_info)) {
        return 0;
    }

    return 1;
}

void aurelion_kernel_main(const struct aurelion_boot_info *boot_info)
{
    struct aurelion_console console;
    struct aurelion_framebuffer_surface framebuffer;
    void *framebuffer_mapping = 0;
    int framebuffer_available = 0;
    aurelion_console_init(&console, VGA_TEXT_BUFFER);

    /* Initialize serial port for CI-visible diagnostics */
    aurelion_serial_init();

    print_serial(&console, AURELION_KERNEL_NAME " kernel ");
    print_serial(&console, AURELION_KERNEL_VERSION);
    print_serial(&console, "\n");

    if (aurelion_validate_boot_info(boot_info)) {
        print_serial(&console, "Boot ABI validated.\n");
        framebuffer_available = aurelion_framebuffer_init(
            &framebuffer, &boot_info->framebuffer);
        if (framebuffer_available) {
            aurelion_serial_write("Prism framebuffer: available.\n");
        }
    } else {
        print_serial(&console, "ERROR: Boot ABI validation failed.\n");
    }

    /* Print boot info details */
    if (boot_info) {
        aurelion_serial_write("  Memory regions: ");
        serial_print_u64(boot_info->memory_region_count);
        aurelion_serial_write("\n");
        aurelion_serial_write("  Kernel phys base: ");
        serial_print_u64(boot_info->kernel_physical_base);
        aurelion_serial_write("\n");
        aurelion_serial_write("  RSDP physical: ");
        serial_print_u64(boot_info->rsdp_physical);
        aurelion_serial_write("\n");
        if (boot_info->framebuffer.address) {
            aurelion_serial_write("  Framebuffer: ");
            serial_print_u64(boot_info->framebuffer.width);
            aurelion_serial_write("x");
            serial_print_u64(boot_info->framebuffer.height);
            aurelion_serial_write(" bpp=");
            serial_print_u64(boot_info->framebuffer.bpp);
            aurelion_serial_write("\n");
        }
    }

    /* Initialize GDT */
    print_serial(&console, "GDT: initializing...\n");
    aurelion_gdt_init();
    print_serial(&console, "GDT: done.\n");

    /* Initialize IDT */
    print_serial(&console, "IDT: initializing...\n");
    aurelion_idt_init();
    print_serial(&console, "IDT: done (interrupts enabled).\n");

    /* Initialize physical memory manager */
    print_serial(&console, "PMM: initializing...\n");
    aurelion_pmm_init(boot_info);
    print_serial(&console, "PMM: done.\n");

    /* Initialize virtual memory manager */
    print_serial(&console, "VMM: initializing...\n");
    framebuffer_mapping = aurelion_vmm_init(
        framebuffer_available ? &boot_info->framebuffer : 0);
    print_serial(&console, "VMM: done.\n");

    if (framebuffer_available) {
        if (framebuffer_mapping) {
            aurelion_framebuffer_rebase(&framebuffer, framebuffer_mapping);
            aurelion_framebuffer_draw_boot_splash(&framebuffer, 4);
            aurelion_serial_write("Prism framebuffer: boot canvas rendered.\n");
        } else {
            aurelion_serial_write("Prism framebuffer: mapping failed.\n");
        }
    }

    /* Initialize kernel heap (use 4 MiB starting at 4 MiB physical, identity mapped) */
    print_serial(&console, "HEAP: initializing...\n");
    void *heap_start = (void *)(uintptr_t)(4 * 1024 * 1024);
    aurelion_heap_init(heap_start, 4 * 1024 * 1024);
    print_serial(&console, "HEAP: done.\n");

    /* Test heap allocation */
    void *test_alloc = aurelion_heap_alloc(64);
    if (test_alloc) {
        aurelion_serial_write("HEAP: test alloc of 64 bytes succeeded.\n");
        aurelion_heap_free(test_alloc);
        aurelion_serial_write("HEAP: test free succeeded.\n");
    } else {
        aurelion_serial_write("HEAP: test alloc FAILED.\n");
    }

    /* All subsystems initialized */
    print_serial(&console, "\n=== Aurelion kernel bring-up complete ===\n");
    aurelion_serial_write("All subsystems initialized. Halting.\n");

    /* Halt */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
