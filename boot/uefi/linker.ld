/* UEFI application linker script for PE-COFF format */
ENTRY(efi_main)

SECTIONS
{
    . = 0;
    .text : {
        *(.text.efi_main)
        *(.text*)
    }
    .rodata : ALIGN(16) { *(.rodata*) }
    .data : ALIGN(16) { *(.data*) }
    .bss : ALIGN(16) {
        *(.bss*)
        *(COMMON)
    }
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
