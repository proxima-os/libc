#ifndef _SIGNAL_H
#define _SIGNAL_H 1

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __clang__
typedef int sig_atomic_t;

#if __INT_WIDTH__ != __SIG_ATOMIC_WIDTH__
#error "__INT_WIDTH__ does not match __SIG_ATOMIC_WIDTH__"
#endif
#else
typedef __SIG_ATOMIC_TYPE__ sig_atomic_t;
#endif

typedef void (*__sig_handler_t)(int);

#define SIG_ERR ((__sig_handler_t) -1)
#define SIG_DFL ((__sig_handler_t) 0)
#define SIG_IGN ((__sig_handler_t) 1)

#define SIGABRT 0
#define SIGFPE 1
#define SIGILL 2
#define SIGSEGV 3
#define SIGTERM 4

__sig_handler_t signal(int __sig, __sig_handler_t __func);

int raise(int __sig);

#ifdef __cplusplus
};
#endif

#endif /* _SIGNAL_H */
