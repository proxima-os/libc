#include "signal.h"
#include "compiler.h"
#include "errno.h"
#include <stddef.h>

// TODO: Integrate with kernel

#define SIG_MIN 0
#define SIG_MAX SIGTERM

static __sig_handler_t handlers[SIG_MAX - SIG_MIN + 1];

EXPORT __sig_handler_t signal(int sig, __sig_handler_t func) {
    if (sig < SIG_MIN || sig > SIG_MAX || func == SIG_ERR) {
        errno = ERR_INVALID_ARGUMENT;
        return SIG_ERR;
    }

    return __atomic_exchange_n(&handlers[sig], func, __ATOMIC_ACQ_REL);
}

EXPORT int raise(int sig) {
    if (sig < SIG_MIN || sig > SIG_MAX) {
        errno = ERR_INVALID_ARGUMENT;
        return 1;
    }

    __sig_handler_t handler = __atomic_exchange_n(&handlers[sig], NULL, __ATOMIC_ACQ_REL);
    if (handler) handler(sig);
    return 0;
}
