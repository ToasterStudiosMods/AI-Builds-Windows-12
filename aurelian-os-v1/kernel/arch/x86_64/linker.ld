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

    /* Discard junk we don't need in a freestanding kernel. */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note .note.*)
    }
}
