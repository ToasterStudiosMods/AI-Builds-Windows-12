/*
 * 32-bit bootstrap for multiboot entry.
 * Sets up flat 32-bit GDT, calls C multiboot_main,
 * then provides stage1_enter_long_mode for the 32->64 transition.
 */

.section .multiboot, "a"
.long 0x1BADB002
.long 0x00000003
.long -(0x1BADB002 + 0x00000003)
.long 0

.section .text.bootstrap, "ax"
.global _start
.extern multiboot_main

_start:
    cli
    lgdt %cs:flat_gdt_ptr

    mov $0x08, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    mov $0x400000, %esp
    xor %ebp, %ebp

    push %ebx
    push %eax
    call multiboot_main

.halt:
    hlt
    jmp .halt

.section .text.transition, "ax"
.global stage1_enter_long_mode
.extern stage1_pml4_addr
.extern stage1_boot_info_addr
.extern stage1_kernel_entry

stage1_enter_long_mode:
    /* Load values from C globals */
    mov stage1_pml4_addr, %eax
    mov stage1_boot_info_addr, %esi
    mov stage1_kernel_entry, %edi

    /* 1. Enable PAE in CR4 */
    mov %cr4, %ecx
    or $0x20, %ecx
    mov %ecx, %cr4

    /* 2. Load PML4 into CR3 */
    mov %eax, %cr3

    /* 3. Enable Long Mode in EFER MSR */
    mov $0xC0000080, %ecx
    rdmsr
    or $0x100, %eax
    wrmsr

    /* 4. Enable Paging (also enters compatibility mode) */
    mov %cr0, %eax
    or $0x80010000, %eax
    mov %eax, %cr0

    /* 5. Load 64-bit GDT */
    lgdt %cs:gdt64_ptr

    /* 6. Far jump to 64-bit code */
    ljmp $0x08, $trampoline64

.code64
trampoline64:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* Jump to kernel: rdi = boot_info, rax = kernel entry */
    mov %rsi, %rdi
    mov %edi, %eax
    jmp *%rax

.code32

.section .data
.align 4

flat_gdt_start:
    .long 0, 0
    .long 0x0000FFFF, 0x00CF9A00
    .long 0x0000FFFF, 0x00CF9200
flat_gdt_end:
flat_gdt_ptr:
    .word flat_gdt_end - flat_gdt_start - 1
    .long flat_gdt_start

gdt64_start:
    .quad 0x0000000000000000
    .quad 0x00209A000000FFFF
    .quad 0x000092000000FFFF
gdt64_end:
gdt64_ptr:
    .word gdt64_end - gdt64_start - 1
    .long gdt64_start
    .long 0

.section .note.GNU-stack, "", @progbits
