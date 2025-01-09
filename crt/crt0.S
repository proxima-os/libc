.text

.balign 16
.globl _start
.hidden _start
.type _start, @function
_start:
    .cfi_startproc
    .cfi_undefined rip

    xor %ebp, %ebp

    lea main(%rip), %rdi
    mov %rsp, %rsi
    # %rdx is already set
    lea _init(%rip), %rcx
    lea _fini(%rip), %r8
    call *__libc_start@GOTPCREL(%rip)
    ud2

    .cfi_endproc
.size _start, . - _start
