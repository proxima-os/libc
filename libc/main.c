#include "auxv.h"
#include "compiler.h"
#include "stdio.p.h"
#include "stdlib.p.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

__attribute__((used)) EXPORT _Noreturn void __libc_start(
        int (*main)(int, char **, char **),
        char **start_info,
        void (*exitfn)(void),
        void (*initfn)(void),
        void (*finifn)(void)
) {
    int argc = (int)(uintptr_t)start_info[0];
    char **argv = &start_info[1];
    char **envp = &argv[argc + 1];

    environ = envp;

    size_t envc = 0;
    while (envp[envc] != NULL) envc += 1;
    init_auxv(&envp[envc + 1]);

    init_stdio();

    if (exitfn) atexit(exitfn);

    initfn();
    int status = main(argc, argv, envp);
    finifn();

    exit(status);
}
