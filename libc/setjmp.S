.text

.balign 16
.globl setjmp
.protected setjmp
.type setjmp, @function
setjmp:
    mov %rbx, 0x00(%rdi)
    mov %rbp, 0x08(%rdi)
    mov %rsp, 0x10(%rdi)
    mov %r12, 0x18(%rdi)
    mov %r13, 0x20(%rdi)
    mov %r14, 0x28(%rdi)
    mov %r15, 0x30(%rdi)

    xor %eax, %eax
    ret
.size setjmp, . - setjmp

.balign 16
.globl longjmp
.protected longjmp
.type longjmp, @function
longjmp:
    mov 0x00(%rdi), %rbx
    mov 0x08(%rdi), %rbp
    mov 0x10(%rdi), %rsp
    mov 0x18(%rdi), %r12
    mov 0x20(%rdi), %r13
    mov 0x28(%rdi), %r14
    mov 0x30(%rdi), %r15

    mov %esi, %eax
    test %eax, %eax
    jnz 1f
    mov $1, %eax
1:  ret
.size longjmp, . - longjmp
