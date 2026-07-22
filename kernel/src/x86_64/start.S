.section .text.bootstrap, "ax"
.global _start
.extern aurelion_kernel_main

_start:
    cli
    mov %rdi, %rdi
    call aurelion_kernel_main
.hang:
    hlt
    jmp .hang

.section .note.GNU-stack,"",@progbits
