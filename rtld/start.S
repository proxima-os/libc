.text

.balign 16
.globl rtld_start
.type rtld_start, @function
rtld_start:
    mov %rsp, %rdi
    mov %rsp, start_rsp(%rip)
    lea _DYNAMIC(%rip), %rsi
    call rtld_init
    xor %edx, %edx
    jmp _start
.size rtld_start, . - rtld_start

.balign 16
.globl rtld_handover
.type rtld_handover, @function
rtld_handover:
    mov %rsi, %rsp
    xor %edx, %edx
    jmp *%rdi
.size rtld_handover, . - rtld_handover
