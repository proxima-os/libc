.text

.balign 16
.globl rtld_start
.hidden rtld_start
.type rtld_start, @function
rtld_start:
    mov %rsp, %rdi
    lea _DYNAMIC(%rip), %rsi
    call rtld_init
    jmp _start
.size rtld_start, . - rtld_start
