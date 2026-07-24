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
    lgdt    (%rdi)               /* arg0: &gdt_ptr (System V AMD64: rdi) */

    /* reload data segments (0x10 = data selector) */
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    movw    %ax, %ss

    /* Reload CS via a 64-bit far return (lretq).
     *
     * We deliberately avoid an indirect far JMP (`ljmp *(%rax)`) here: in long
     * mode that form defaults to a 32-bit offset (m16:32) unless a REX.W prefix
     * is emitted, so an 8-byte offset operand is mis-parsed and the selector is
     * read from the wrong bytes — yielding a null CS selector, a #GP, and
     * (with no IDT yet) a triple fault. Pushing selector:offset and issuing
     * `lretq` loads CS:RIP atomically with unambiguous 64-bit operand size. */
    leaq    .reload_cs(%rip), %rax
    pushq   $0x08                /* new CS selector (index 1, RPL 0) */
    pushq   %rax                 /* new RIP                          */
    lretq
.reload_cs:
    ret
    .size   gdt_flush, . - gdt_flush
