#ifndef _SETJMP_H
#define _SETJMP_H 1

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned long __rbx;
    unsigned long __rbp;
    unsigned long __rsp;
    unsigned long __r12;
    unsigned long __r13;
    unsigned long __r14;
    unsigned long __r15;
} jmp_buf[1];

__attribute__((__returns_twice__)) int setjmp(jmp_buf __buf);
#define setjmp setjmp

__attribute__((__noreturn__)) void longjmp(jmp_buf __buf);

#ifdef __cplusplus
};
#endif

#endif /* _SETJMP_H */
