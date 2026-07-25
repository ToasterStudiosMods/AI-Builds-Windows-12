/* ============================================================================
 * Aurelian OS — Aurelion kernel
 * arch/x86_64/isr.S — interrupt/exception entry stubs
 *
 * 48 stubs (vectors 0-47) that normalise the stack to
 *   [vector][error-code][rip][cs][rflags][rsp][ss]
 * then jump to a common path that saves all GPRs and calls interrupt_common().
 *
 * Exceptions that push a CPU error code (8,10,11,12,13,14,17,21) use ISR_ERR;
 * all others push a dummy 0 so every frame has the same shape.
 * ==========================================================================*/

    .code64
    .section .text

    .macro ISR_NOERR num
isr_stub_\num:
    pushq   $0
    pushq   $\num
    jmp     isr_common
    .endm

    .macro ISR_ERR num
isr_stub_\num:
    pushq   $\num
    jmp     isr_common
    .endm

    .macro IRQ_STUB num
isr_stub_\num:
    pushq   $0
    pushq   $\num
    jmp     isr_common
    .endm

    /* --- CPU exceptions 0-31 --- */
    ISR_NOERR 0
    ISR_NOERR 1
    ISR_NOERR 2
    ISR_NOERR 3
    ISR_NOERR 4
    ISR_NOERR 5
    ISR_NOERR 6
    ISR_NOERR 7
    ISR_ERR   8
    ISR_NOERR 9
    ISR_ERR   10
    ISR_ERR   11
    ISR_ERR   12
    ISR_ERR   13
    ISR_ERR   14
    ISR_NOERR 15
    ISR_NOERR 16
    ISR_ERR   17
    ISR_NOERR 18
    ISR_NOERR 19
    ISR_NOERR 20
    ISR_ERR   21
    ISR_NOERR 22
    ISR_NOERR 23
    ISR_NOERR 24
    ISR_NOERR 25
    ISR_NOERR 26
    ISR_NOERR 27
    ISR_NOERR 28
    ISR_NOERR 29
    ISR_NOERR 30
    ISR_NOERR 31

    /* --- Hardware IRQs 0-15 -> vectors 32-47 --- */
    IRQ_STUB 32
    IRQ_STUB 33
    IRQ_STUB 34
    IRQ_STUB 35
    IRQ_STUB 36
    IRQ_STUB 37
    IRQ_STUB 38
    IRQ_STUB 39
    IRQ_STUB 40
    IRQ_STUB 41
    IRQ_STUB 42
    IRQ_STUB 43
    IRQ_STUB 44
    IRQ_STUB 45
    IRQ_STUB 46
    IRQ_STUB 47

    /* --- common path --- */
    .extern interrupt_common
isr_common:
    push    %rax
    push    %rbx
    push    %rcx
    push    %rdx
    push    %rsi
    push    %rdi
    push    %rbp
    push    %r8
    push    %r9
    push    %r10
    push    %r11
    push    %r12
    push    %r13
    push    %r14
    push    %r15

    /* 15 GPRs pushed (120 bytes): vector at rsp+120, error code at rsp+128. */
    movq    120(%rsp), %rdi        /* arg1 = vector            */
    movq    128(%rsp), %rsi        /* arg2 = error code        */
    movq    %rsp, %rdx             /* arg3 = interrupted stack */
    cld
    call    interrupt_common

    /* interrupt_common returns the stack to resume on. Normally that is the one
     * we came in on; when the scheduler decides to preempt, it is a different
     * task's stack, and the pops plus iretq below therefore resume that thread
     * instead. This one instruction is the whole context switch. */
    movq    %rax, %rsp

    pop     %r15
    pop     %r14
    pop     %r13
    pop     %r12
    pop     %r11
    pop     %r10
    pop     %r9
    pop     %r8
    pop     %rbp
    pop     %rdi
    pop     %rsi
    pop     %rdx
    pop     %rcx
    pop     %rbx
    pop     %rax

    addq    $16, %rsp              /* discard vector + error code */
    iretq

    /* --- table of stub addresses, indexed by vector (used by idt.c) --- */
    .section .rodata
    .align 8
    .global isr_stub_table
isr_stub_table:
    .quad isr_stub_0,  isr_stub_1,  isr_stub_2,  isr_stub_3
    .quad isr_stub_4,  isr_stub_5,  isr_stub_6,  isr_stub_7
    .quad isr_stub_8,  isr_stub_9,  isr_stub_10, isr_stub_11
    .quad isr_stub_12, isr_stub_13, isr_stub_14, isr_stub_15
    .quad isr_stub_16, isr_stub_17, isr_stub_18, isr_stub_19
    .quad isr_stub_20, isr_stub_21, isr_stub_22, isr_stub_23
    .quad isr_stub_24, isr_stub_25, isr_stub_26, isr_stub_27
    .quad isr_stub_28, isr_stub_29, isr_stub_30, isr_stub_31
    .quad isr_stub_32, isr_stub_33, isr_stub_34, isr_stub_35
    .quad isr_stub_36, isr_stub_37, isr_stub_38, isr_stub_39
    .quad isr_stub_40, isr_stub_41, isr_stub_42, isr_stub_43
    .quad isr_stub_44, isr_stub_45, isr_stub_46, isr_stub_47

    .section .note.GNU-stack, "", @progbits
