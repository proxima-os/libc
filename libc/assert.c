#include "assert.h"
#include "compiler.h"
#include <stdio.h>

EXPORT __attribute__((noreturn)) void __assert_fail(const char *expr, const char *file, int line) {
    __builtin_fprintf(stderr, "assertion `%s` failed at %s:%d\n", expr, file, line);
    fflush(stderr);
    __builtin_abort();
}
