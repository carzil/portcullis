    .text
    .global __save_context
__save_context:
    /* save preserved registers */
    mov %rbx, (%rdi)
    mov %rbp, 8(%rdi)
    mov %r12, 16(%rdi)
    mov %r13, 24(%rdi)
    mov %r14, 32(%rdi)
    mov %r15, 40(%rdi)

    /* save stack pointer */
    lea 8(%rsp), %rax
    mov %rax, 48(%rdi)

    /* get rip from return address */
    mov (%rsp), %rax
    mov %rax, 56(%rdi)

    mov $0, %rax
    ret

    .text
    .global __switch_to
__switch_to:
    mov (%rdi), %rbx
    mov 8(%rdi), %rbp
    mov 16(%rdi), %r12
    mov 24(%rdi), %r13
    mov 32(%rdi), %r14
    mov 40(%rdi), %r15

    mov 48(%rdi), %rsp
    mov 56(%rdi), %rdx

    /* always return 1 in __save_context */
    mov $1, %rax
    jmp *%rdx
