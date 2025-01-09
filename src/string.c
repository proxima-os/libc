#include "string.h"
#include "compiler.h"
#include <stdint.h>

EXPORT int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *b1 = s1;
    const unsigned char *b2 = s2;

    while (n--) {
        unsigned char c1 = *b1++;
        unsigned char c2 = *b2++;

        if (c1 < c2) return -1;
        if (c1 > c2) return 1;
    }

    return 0;
}

EXPORT void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;

    while (n--) {
        *d++ = *s++;
    }

    return dest;
}

EXPORT void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;

    if ((uintptr_t)d < (uintptr_t)s) {
        while (n--) {
            *d++ = *s++;
        }
    } else if ((uintptr_t)d > (uintptr_t)s) {
        d += n;
        s += n;

        while (n--) {
            *--d = *--s;
        }
    }

    return dest;
}

EXPORT void *memset(void *dest, int value, size_t n) {
    unsigned char *d = dest;
    unsigned char c = value;

    while (n--) {
        *d++ = c;
    }

    return dest;
}

EXPORT int strcmp(const char *s1, const char *s2) {
    for (;;) {
        unsigned char c1 = *s1++;
        unsigned char c2 = *s2++;

        if (c1 < c2) return -1;
        if (c1 > c2) return 1;
        if (c1 == 0) return 0;
    }
}
