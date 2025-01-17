#include "assert.h"
#include "compiler.h"
#include "errno.h"
#include "stdlib.h"
#include <hydrogen/memory.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    size_t size;
} alloc_meta_t;

typedef struct free_obj {
    struct free_obj *next;
} free_obj_t;

#define META_OFF ((sizeof(alloc_meta_t) + (_Alignof(max_align_t) - 1)) & ~(_Alignof(max_align_t) - 1))
#define ZERO_PTR ((void *)_Alignof(max_align_t))

#define MAX_ORDER 12
#define ALLOC_GRAN (1ul << MAX_ORDER)

static free_obj_t *free_objects[MAX_ORDER];

static int get_order_from_size(size_t size) {
    return 64 - __builtin_clzl(size - 1);
}

static void *alloc_order(int order) {
    free_obj_t *obj = free_objects[order];

    if (obj) {
        free_objects[order] = obj->next;
        return obj;
    }

    intptr_t addr = hydrogen_map_memory(0, ALLOC_GRAN, VMM_PRIVATE | VMM_WRITE, -1, 0);
    if (addr < 0) {
        errno = -addr;
        return NULL;
    }

    free_obj_t *objs = (void *)addr;
    free_obj_t *last = objs;
    size_t size = 1ul << order;

    for (size_t cur = size; cur < ALLOC_GRAN; cur += size) {
        free_obj_t *obj = (void *)addr + cur;
        last->next = obj;
        last = obj;
    }

    last->next = NULL;
    free_objects[order] = objs->next;

    return objs;
}

static void free_order(void *ptr, int order) {
    free_obj_t *obj = ptr;
    obj->next = free_objects[order];
    free_objects[order] = obj;
}

static void *alloc_large(size_t size) {
    size = (size + (ALLOC_GRAN - 1)) & ~(ALLOC_GRAN - 1);

    intptr_t addr = hydrogen_map_memory(0, size, VMM_PRIVATE | VMM_WRITE, 0, 0);
    if (addr < 0) {
        errno = -addr;
        return NULL;
    }

    return (void *)addr;
}

static bool realloc_large(void **ptr, size_t old, size_t new) {
    old = (old + (ALLOC_GRAN - 1)) & ~(ALLOC_GRAN - 1);
    new = (new + (ALLOC_GRAN - 1)) & ~(ALLOC_GRAN - 1);

    if (old == new) return true;

    if (old < new) {
        UNUSED int error = hydrogen_unmap_memory((uintptr_t)*ptr + new, old - new);
        assert(error == 0);
        return true;
    }

    intptr_t res = hydrogen_map_memory((uintptr_t)*ptr + old, new - old, VMM_PRIVATE | VMM_WRITE | VMM_TRY_EXACT, 0, 0);
    return res >= 0;
}

static void free_large(void *ptr, size_t size) {
    size = (size + (ALLOC_GRAN - 1)) & ~(ALLOC_GRAN - 1);
    hydrogen_unmap_memory((uintptr_t)ptr, size);
}

EXPORT void *malloc(size_t size) {
    if (size == 0) return ZERO_PTR;
    size += META_OFF;

    alloc_meta_t *meta = size <= ALLOC_GRAN ? alloc_order(get_order_from_size(size)) : alloc_large(size);
    if (!meta) return NULL;

    meta->size = size;
    return (void *)meta + META_OFF;
}

EXPORT void *realloc(void *ptr, size_t size) {
    if (ptr == NULL || ptr == ZERO_PTR) return __builtin_malloc(size);
    if (size == 0) {
        __builtin_free(ptr);
        return ZERO_PTR;
    }

    alloc_meta_t *meta = ptr - META_OFF;

    size_t user_old = meta->size - META_OFF;
    size_t user_new = size;
    size += META_OFF;

    if (meta->size <= ALLOC_GRAN && size <= ALLOC_GRAN) {
        int old_order = get_order_from_size(meta->size);
        int new_order = get_order_from_size(size);

        if (old_order == new_order) return ptr;
    } else if (meta->size > ALLOC_GRAN && size > ALLOC_GRAN && realloc_large(&ptr, meta->size, size)) {
        return ptr;
    }

    void *new_alloc = __builtin_malloc(user_new);
    if (!new_alloc) return NULL;
    __builtin_memcpy(new_alloc, ptr, user_old < user_new ? user_old : user_new);
    __builtin_free(ptr);
    return new_alloc;
}

EXPORT void free(void *ptr) {
    if (ptr == NULL || ptr == ZERO_PTR) return;

    alloc_meta_t *meta = ptr - META_OFF;

    if (meta->size <= ALLOC_GRAN) {
        free_order(meta, get_order_from_size(meta->size));
    } else {
        free_large(meta, meta->size);
    }
}

EXPORT void *calloc(size_t nmemb, size_t size) {
    void *ptr = __builtin_malloc(nmemb * size);
    if (ptr) __builtin_memset(ptr, 0, nmemb * size);
    return ptr;
}
