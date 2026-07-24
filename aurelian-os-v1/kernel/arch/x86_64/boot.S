/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * arch/x86_64/boot.S
 *
 * Multiboot2 boot header + 32-bit entry, transitions to 64-bit long mode.
 *
 * GRUB (or any Multiboot2 loader) validates the header below, loads this ELF
 * at 1 MiB physical, and jumps to `_start` in 32-bit protected mode with
 * paging disabled. We then:
 *
 *   1. Set up a flat 32-bit GDT (temporary) and stack.
 *   2. Build a 4-level page table (PML4 → PDPT → PD) identity-mapping the
 *      first 1 GiB of physical memory using 2 MiB pages.
 *   3. Enable PAE (CR4.PAE).
 *   4. Load CR3 with the PML4 address.
 *   5. Set EFER.LME (long-mode enable) in MSR 0xC0000080.
 *   6. Enable paging (CR0.PG) — CPU is now in compatibility mode.
 *   7. Load the 64-bit GDT and far-jump to a 64-bit code segment → long mode.
 *   8. Call kmain (now 64-bit) with the multiboot2 info pointer.
 *
 * This matches the blueprint: "v1 kernel: original x86-64 kernel",
 * "CPU target: x86-64-v2", "Memory manager: four-level page tables for v1".
 *
 * Assembled by GCC (GNU as / AT&T syntax). No nasm required.
 * ==========================================================================*/

    .set MB2_MAGIC,      0xe85250d6
    .set MB2_ARCH_I386,  0
    .set MB2_MAGIC_LOAD, 0x36d76289

    /* ------------------------------------------------------------------ */
    /* Multiboot2 header                                                  */
    /* ------------------------------------------------------------------ */
    .section .multiboot
    .align 8
mb2_header:
    .long   MB2_MAGIC
    .long   MB2_ARCH_I386
    .long   mb2_header_end - mb2_header
    .long   0x100000000 - (MB2_MAGIC + MB2_ARCH_I386 + (mb2_header_end - mb2_header))

    /* framebuffer request tag: ask GRUB for a linear RGB framebuffer.
     * width/height/depth are preferred values; GRUB picks the closest mode
     * and reports the actual geometry back in the mbi2 framebuffer tag. */
    .align 8
    .short  5                    /* type = framebuffer            */
    .short  0                    /* flags = 0 (required)          */
    .long   20                   /* size                          */
    .long   1024                 /* preferred width               */
    .long   768                  /* preferred height              */
    .long   32                   /* preferred bpp                 */

    /* end tag */
    .align 8
    .short  0
    .short  0
    .long   8
mb2_header_end:

    /* ------------------------------------------------------------------ */
    /* Page tables (identity-map first 4 GiB with 2 MiB pages)            */
    /* Placed in .bss-low so they live at a known low physical address.   */
    /* ------------------------------------------------------------------ */
    .section .bss.low
    .align 4096
pml4:
    .skip 4096
pdpt:
    .skip 4096
    /* Four page directories: identity-map the first 4 GiB with 2 MiB pages,
     * so a linear framebuffer placed high in the physical map (VirtualBox's
     * VBE LFB sits around 0xE0000000) is reachable by the kernel. */
pd0:
    .skip 4096
pd1:
    .skip 4096
pd2:
    .skip 4096
pd3:
    .skip 4096

    /* 16 KiB bootstrap stack */
    .align 16
stack_bottom:
    .skip 16384
stack_top:

    /* ------------------------------------------------------------------ */
    /* 32-bit entry point (protected mode, paging off)                    */
    /* ------------------------------------------------------------------ */
    .section .text
    .code32
    .global _start
    .type   _start, @function
_start:
    /* ebx = mbi2 info pointer (Multiboot2 entry convention). Preserve it in
     * esi: the page-table bring-up below clobbers edi (movl $pml4,%edi /
     * rep stosl / movl $pd,%edi), and esi is untouched through both the page
     * setup and the long-mode transition. */
    movl    %ebx, %esi

    /* Set up a 32-bit stack. */
    movl    $stack_top, %esp
    and     $-16, %esp

    /* Load the temporary 32-bit GDT (null + 32-bit code + data). */
    lgdt    gdtr32

    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    movw    %ax, %ss
    ljmp    $0x08, $.1
.1:

    /* --- Build page tables: identity-map first 4 GiB with 2 MiB pages. --- */

    /* Zero PML4, PDPT and pd0..pd3 (6 pages x 1024 dwords = 6144). */
    movl    $pml4, %edi
    xorl    %eax, %eax
    movl    $6144, %ecx
    rep     stosl

    /* PML4[0] -> PDPT (present | writable). */
    movl    $pdpt, %eax
    orl     $0x003, %eax
    movl    %eax, (pml4)

    /* PDPT[0..3] -> pd0..pd3 (each covers 1 GiB). */
    movl    $pd0, %eax
    orl     $0x003, %eax
    movl    %eax, (pdpt)
    movl    $pd1, %eax
    orl     $0x003, %eax
    movl    %eax, (pdpt + 8)
    movl    $pd2, %eax
    orl     $0x003, %eax
    movl    %eax, (pdpt + 16)
    movl    $pd3, %eax
    orl     $0x003, %eax
    movl    %eax, (pdpt + 24)

    /* Fill pd0..pd3 as one contiguous run of 2048 entries: identity-map the
     * first 4 GiB with 2 MiB pages. Entry = phys | present | writable | huge.
     * Stride is 2 MiB (0x200000); phys stays < 4 GiB so the high dword (already
     * zeroed above) remains 0. */
    movl    $pd0, %edi
    movl    $0x00000083, %eax    /* phys 0 | present | writable | huge (PS) */
    movl    $2048, %ecx
.fill_pd:
    movl    %eax, (%edi)
    addl    $0x00200000, %eax    /* next 2 MiB */
    addl    $8, %edi
    decl    %ecx
    jnz     .fill_pd

    /* --- Enable PAE (CR4.PAE = bit 5). --- */
    movl    %cr4, %eax
    orl     $(1 << 5), %eax
    movl    %eax, %cr4

    /* --- Load CR3 with PML4 address. --- */
    movl    $pml4, %eax
    movl    %eax, %cr3

    /* --- Set EFER.LME (long-mode enable). MSR 0xC0000080, bit 8. --- */
    movl    $0xC0000080, %ecx
    rdmsr
    orl     $(1 << 8), %eax
    wrmsr

    /* --- Enable paging (CR0.PG = bit 31). CPU now in compatibility mode. --- */
    movl    %cr0, %eax
    orl     $(1 << 31), %eax
    movl    %eax, %cr0

    /* --- Load the 64-bit GDT and far-jump to 64-bit code segment. --- */
    lgdt    gdtr64
    ljmp    $0x08, $long_mode_start

    /* Should never reach here. */
    cli
1:  hlt
    jmp     1b
    .size   _start, . - _start

    /* ------------------------------------------------------------------ */
    /* 64-bit entry point (long mode).                                    */
    /* ------------------------------------------------------------------ */
    .section .text
    .code64
    .global long_mode_start
    .type   long_mode_start, @function
long_mode_start:
    /* Reload data segments with the 64-bit data selector (0x10). */
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    movw    %ax, %ss

    /* Set up a fresh 64-bit stack (16-byte aligned per the SysV AMD64 ABI). */
    movabsq $stack_top, %rsp
    andq    $-16, %rsp

    /* Pass the preserved mbi2 info pointer (esi) as the first argument (rdi)
     * to kmain. movl zero-extends esi into rdi; the pointer is a <4 GiB
     * physical address. */
    movl    %esi, %edi
    call    kmain

    cli
2:  hlt
    jmp     2b
    .size   long_mode_start, . - long_mode_start

    /* ------------------------------------------------------------------ */
    /* Temporary 32-bit GDT (used only to reach long mode).              */
    /* ------------------------------------------------------------------ */
    .section .rodata
    .align 8
gdt32:
    .quad 0x0000000000000000               /* 0x00 null                     */
    .quad 0x00CF9A000000FFFF               /* 0x08 32-bit code, ring 0      */
    .quad 0x00CF92000000FFFF               /* 0x10 32-bit data, ring 0      */
gdt32_end:

gdtr32:
    .word gdt32_end - gdt32 - 1
    .long gdt32

    /* 64-bit GDT (null + 64-bit code + data). */
    .align 8
gdt64:
    .quad 0x0000000000000000               /* 0x00 null                     */
    .quad 0x00AF9A000000FFFF               /* 0x08 64-bit code, ring 0 (L)  */
    .quad 0x00CF92000000FFFF               /* 0x10 64-bit data, ring 0      */
gdt64_end:

gdtr64:
    .word gdt64_end - gdt64 - 1
    .quad  gdt64
