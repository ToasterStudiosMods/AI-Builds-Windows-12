/* Linker script for multiboot stage-1 bootloader (32-bit) */
ENTRY(multiboot_main)

SECTIONS
{
    . = 1M;

    .multiboot : ALIGN(4K) {
        *(.multiboot)
    }

    .text : ALIGN(4K) {
        *(.text*)
    }

    .rodata : ALIGN(4K) {
        *(.rodata*)
    }

    .data : ALIGN(4K) {
        *(.data*)
    }

    .bss : ALIGN(4K) {
        *(COMMON)
        *(.bss*)
    }
}
