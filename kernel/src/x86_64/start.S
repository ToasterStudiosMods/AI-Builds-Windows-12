.section .text.bootstrap, "ax"
.global _start
.extern aurelion_kernel_main

_start:
    cli
    /* boot_info pointer is already in %rdi from the loader */
    mov %rdi, %rdi
    call aurelion_kernel_main
.hang:
    hlt
    jmp .hang

.section .note.GNU-stack,"",@progbits
