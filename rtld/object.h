#ifndef RTLD_OBJECT_H
#define RTLD_OBJECT_H

#include "elf.h"
#include <stddef.h>
#include <stdint.h>

typedef struct object {
    struct {
        char *data;
        size_t len;
        uint64_t hash;
    } path;

    struct object *table_prev;
    struct object *table_next;
    struct object *search_next;

    const Elf64_Dyn *dynamic;
    intptr_t slide;

    struct {
        const Elf64_Word *buckets;
        const Elf64_Word *chains;
        Elf64_Word nbuckets;
    } hash;
    const char *strtab;
    const Elf64_Sym *symtab;
    size_t syment;
    const char *rpath;
    const char *runpath;
} object_t;

extern object_t exec_object;
extern object_t rtld_object;
extern object_t vdso_object;

void init_objects(void);

void init_object(object_t *obj);

void process_dependencies(object_t *root);

const Elf64_Sym *search_for_symbol(const char *name, object_t **owner_out);

#endif // RTLD_OBJECT_H
