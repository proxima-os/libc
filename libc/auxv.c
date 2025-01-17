#include "auxv.h"
#include "compiler.h"
#include "elf.h"
#include "sys/auxv.h"

static Elf64_auxv_t *aux_vector;

void init_auxv(void *vector) {
    aux_vector = vector;
}

EXPORT unsigned long getauxval(unsigned long tag) {
    for (Elf64_auxv_t *cur = aux_vector; cur->a_type != AT_NULL; cur++) {
        if ((unsigned long)cur->a_type == tag) {
            return cur->a_un.a_val;
        }
    }

    return 0;
}
