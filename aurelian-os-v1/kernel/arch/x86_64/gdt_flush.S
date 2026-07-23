/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * arch/x86_64/gdt_flush.S — load the GDT and reload segment registers (64-bit)
 *
 * Called as gdt_flush(uint64_t gdtr_addr) from gdt.c, in long mode.
 * ==========================================================================*/

    .section .text
    .code64
    .global gdt_flush
    .type   gdt_flush, @function
gdt_flush:
    movq    %rdi, %rax            /* arg0: &gdt_ptr (System V AMD64) */
    lgdt    (%rax)

    /* reload data segments (0x10 = data selector) */
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    movw    %ax, %ss

    /* indirect far jump to reload CS via a far pointer in memory.
     * [rip+far_jmp_ptr] = { .64 offset ; .16 selector 0x08 } */
    leaq    far_jmp_ptr(%rip), %rax
    ljmp    *(%rax)
.1:
    ret
    .size   gdt_flush, . - gdt_flush

    .section .rodata
    .align 8
far_jmp_ptr:
    .quad   .1
    .short  0x08

