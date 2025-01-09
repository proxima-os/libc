#ifndef _STRING_H
#define _STRING_H 1

#define __need_size_t
#define __need_NULL
#include <stddef.h>

int memcmp(const void *s1, const void *s2, size_t n);
void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *dest, int value, size_t n);

int strcmp(const char *s1, const char *s2);

#endif // _STRING_H
