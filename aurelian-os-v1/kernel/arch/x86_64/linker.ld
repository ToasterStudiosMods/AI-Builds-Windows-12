/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * arch/x86_64/linker.ld
 *
 * Links the kernel as a 64-bit Multiboot2 ELF loaded at 1 MiB physical.
 *
 * The Multiboot2 header MUST come first so the loader can find it within
 * the first 32768 bytes. Two PT_LOAD segments: text (R+X) and data (R+W).
 *
 * v1 loads the kernel at the low 1 MiB mark (identity-mapped by the boot
 * page tables). The .bss.low section holds the bootstrap page tables and
 * stack, which the 32-bit boot code references before long mode is active.
 * ==========================================================================*/

ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);          /* R+X — multiboot + code + rodata   */
    data PT_LOAD FLAGS(6);          /* R+W — data + bss                  */
}

SECTIONS
{
    /* Load the kernel at the conventional 1 MiB mark. */
    . = 1M;

    /* Multiboot header first — must appear early in the file. */
    .boot :
    {
        KEEP(*(.multiboot))
    } :text

    /* Executable code. */
    .text :
    {
        *(.text)
        *(.text.*)
    } :text

    /* Read-only data. */
    .rodata :
    {
        *(.rodata)
        *(.rodata.*)
    } :text

    /* Page-align the data segment start. */
    . = ALIGN(0x1000);

    /* Initialized data. */
    .data :
    {
        *(.data)
        *(.data.*)
    } :data

    /* Bootstrap page tables + stack live here (referenced by boot.S). */
    .bss.low :
    {
        *(.bss.low)
    } :data

    /* General uninitialized data + common symbols. */
    .bss :
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    } :data

    /* End of the loaded image. The kernel bump-allocates its large frame
     * buffers above this (see kernel.c), so they never bloat the ELF's memsz
     * — GRUB then only has to place a ~1 MiB kernel plus the boot modules. */
    . = ALIGN(0x1000);
    _kernel_end = .;

    /* Discard junk we don't need in a freestanding kernel. */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note .note.*)
    }

    /* The Multiboot2 header must sit within the first 32 KiB of the ELF *file*.
     * That only holds while .boot is allocated into the first PT_LOAD segment
     * ahead of .text — if the .multiboot input section ever loses its "a" flag
     * the linker silently orphans it to the end of the file and GRUB reports
     * "no multiboot header found". Fail the build instead. */
    ASSERT(ADDR(.text) >= ADDR(.boot) + SIZEOF(.boot),
           ".boot must be allocated before .text (is .multiboot marked \"a\"?)")
}
