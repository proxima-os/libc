#ifndef _STDLIB_H
#define _STDLIB_H 1

#ifdef __cplusplus
extern "C" {
#endif

#define __need_NULL
#define __need_size_t
#define __need_wchar_t
#include <stddef.h>

typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long quot;
    long rem;
} ldiv_t;

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 0x7fffffff
#define MB_CUR_MAX 4ul

double atof(const char *__nptr);
int atoi(const char *__nptr);
long atol(const char *__nptr);
double strtod(const char *__nptr, char **__endptr);
long strtol(const char *__nptr, char **__endptr, int __base);
unsigned long strtoul(const char *__nptr, char **__endptr, int __base);
int rand(void);
void srand(unsigned __seed);
void *calloc(size_t __nmemb, size_t __size);
void free(void *__ptr);
void *malloc(size_t __size);
void *realloc(void *__ptr, size_t __size);
__attribute__((__noreturn__)) void abort(void);
int atexit(void (*__func)(void));
__attribute__((__noreturn__)) void exit(int __status);
char *getenv(const char *__name);
int system(const char *__string);
void *bsearch(
        const void *__key,
        const void *__base,
        size_t __nmemb,
        size_t __size,
        int (*__compar)(const void *, const void *)
);
void qsort(void *__base, size_t __nmemb, size_t __size, int (*__compar)(const void *, const void *));
int abs(int __j);
div_t div(int __numer, int __denom);
long labs(long __j);
ldiv_t ldiv(long __numer, long __denom);
int mblen(const char *__s, size_t __n);
int mbtowc(wchar_t *__pwc, const char *__s, size_t __n);
int wctomb(char *__s, wchar_t __wchar);
size_t mbstowcs(wchar_t *__pwcs, const char *__s, size_t __n);
size_t wcstombs(char *__s, const wchar_t *__pwcs, size_t __n);

#ifdef __cplusplus
};
#endif

#endif /* _STDLIB_H */
