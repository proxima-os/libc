#include "stdlib.h"
#include "compiler.h"
#include "ctype.h"
#include "errno.h"
#include "hydrogen/sched.h"
#include "limits.h"
#include "math.h"
#include "signal.h"
#include "stdlib.p.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

char **environ;

EXPORT double atof(const char *nptr) {
    return strtod(nptr, NULL);
}

EXPORT int atoi(const char *nptr) {
    return strtol(nptr, NULL, 10);
}

EXPORT long atol(const char *nptr) {
    return strtol(nptr, NULL, 10);
}

typedef struct {
    const char *data;
    size_t length;
} tagged_str_t;

typedef struct {
    bool negative;
    tagged_str_t whole;
    tagged_str_t frac;
    int exponent;
} fp_parts_t;

static int parse_exponent(const char **str) {
    const char *cur = *str;
    if ((cur[0] & ~0x20) != 'E') return 0;

    bool negative = cur[1] == '-';
    if (negative || cur[1] == '+') cur++;

    char c = cur[1];
    if (c < '0' || c > '9') return 0;
    int value = c - '0';

    for (;;) {
        char c = cur[2];
        if (c < '0' || c > '9') break;
        value = value * 10 + (c - '0');
        cur++;
    }

    *str = &cur[2];
    return value;
}

static bool get_fp_parts(const char *str, char **endptr, fp_parts_t *out) {
    while (isspace(*str)) str++;

    out->negative = str[0] == '-';
    if (out->negative || str[0] == '+') str++;

    tagged_str_t *cur = &out->whole;
    cur->data = str;
    size_t num_digits = 0;

    for (;;) {
        char c = *str;

        if (c == '.') {
            if (cur == &out->frac) break;

            cur = &out->frac;
            str++;
            cur->data = str;
            continue;
        }

        if (c < '0' || c > '9') break;

        str++;
        cur->length++;
        num_digits++;
    }

    if (num_digits == 0) return false;

    out->exponent = parse_exponent(&str);
    if (endptr) *endptr = (char *)str;
    return true;
}

// https://en.wikipedia.org/wiki/Exponentiation_by_squaring
static double ipow(double x, int n) {
    if (n < 0) {
        x = 1 / x;
        n = -n;
    } else if (n == 0) {
        return 1;
    }

    double y = 1;
    while (n > 1) {
        if (n & 1) {
            y *= y;
            n -= 1;
        }

        x *= x;
        n >>= 1;
    }

    return x * y;
}

// TODO: Use a proper decimal-to-binary algorithm, since this isn't particularly accurate
EXPORT double strtod(const char *nptr, char **endptr) {
    fp_parts_t parts = {};
    if (!get_fp_parts(nptr, endptr, &parts)) {
        if (endptr) *endptr = (char *)nptr;
        return 0.0;
    }

    double value = 0.0;

    for (size_t i = 0; i < parts.whole.length; i++) {
        value = (value * 10) + (*parts.whole.data++ - '0');
    }

    for (size_t i = 0; i < parts.frac.length; i++) {
        value = (value * 10) + (*parts.frac.data++ - '0');
    }

    value *= ipow(10, parts.exponent - parts.frac.length);

    if (__builtin_isinf(value)) {
        errno = ERANGE;
        return HUGE_VAL;
    }

    return !parts.negative ? value : -value;
}

static int dval(char digit, int base) {
    int value;

    if (digit >= '0' && digit <= '9') {
        value = digit - '0';
    } else {
        digit &= ~0x20; // convert to uppercase if it's a letter
        if (digit >= 'A' && digit <= 'Z') value = digit - 'A' + 10;
        else return -1;
    }

    return value < base ? value : -1;
}

typedef struct {
    unsigned long abs;
    bool negative;
    bool overflow;
} int_parts_t;

static bool get_int_parts(const char *nptr, char **endptr, int base, int_parts_t *out) {
    const char *str = nptr;
    while (isspace(*str)) str++;

    out->negative = str[0] == '-';
    if (out->negative || str[0] == '+') str++;

    switch (base) {
    case 0:
        if (str[0] == '0') {
            if ((str[1] & ~0x20) == 'X' && dval(str[2], 16) >= 0) {
                base = 16;
                str += 2;
            } else {
                base = 8;
                str++;
            }
        } else {
            base = 10;
        }
        break;
    case 16:
        if (str[0] == '0' && (str[1] & ~0x20) == 'X' && dval(str[2], 16) >= 0) str += 2;
        break;
    default: break;
    }

    const char *start = str;

    for (;;) {
        char c = *str;
        int val = dval(c, base);
        if (val < 0) break;

        if (!out->overflow) {
            if (__builtin_umull_overflow(out->abs, base, &out->abs) ||
                __builtin_uaddl_overflow(out->abs, val, &out->abs)) {
                out->overflow = true;
            }
        }

        str++;
    }

    if (str != start) {
        if (endptr) *endptr = (char *)str;
        return true;
    } else {
        if (endptr) *endptr = (char *)nptr;
        return false;
    }
}

EXPORT long strtol(const char *nptr, char **endptr, int base) {
    int_parts_t parts;
    if (!get_int_parts(nptr, endptr, base, &parts)) return 0;

    if (!parts.overflow) {
        if (!parts.negative) {
            if (parts.abs <= LONG_MAX) {
                return parts.abs;
            }
        } else {
            if (parts.abs <= -((unsigned long)LONG_MIN)) {
                return -parts.abs;
            }
        }
    }

    errno = ERANGE;
    return !parts.negative ? LONG_MAX : LONG_MIN;
}

EXPORT unsigned long strtoul(const char *nptr, char **endptr, int base) {
    int_parts_t parts;
    if (!get_int_parts(nptr, endptr, base, &parts)) return 0;

    if (!parts.overflow) {
        return !parts.negative ? parts.abs : -parts.abs;
    } else {
        errno = ERANGE;
        return ULONG_MAX;
    }
}

#define RAND_LCG_MULT 25214903917
#define RAND_LCG_INCR 11
#define RAND_LCG_BITS 48
#define RAND_OUT_OFFS 16

static uint64_t rand_state = 1;

EXPORT int rand(void) {
    rand_state = (rand_state * RAND_LCG_MULT + RAND_LCG_INCR) & ((1ul << RAND_LCG_BITS) - 1);
    return (rand_state >> RAND_OUT_OFFS) & RAND_MAX;
}

EXPORT void srand(unsigned seed) {
    rand_state = seed;
}

EXPORT __attribute__((__noreturn__)) void abort(void) {
    raise(SIGABRT);
    hydrogen_exit();
}

typedef struct atexit {
    void (*func)(void);
    struct atexit *next;
} atexit_t;

static atexit_t *atexit_funcs;

EXPORT int atexit(void (*func)(void)) {
    atexit_t *data = __builtin_malloc(sizeof(*data));
    if (!data) return -1;

    data->func = func;
    data->next = atexit_funcs;
    atexit_funcs = data;

    return 0;
}

EXPORT __attribute__((__noreturn__)) void exit(UNUSED int status) {
    while (atexit_funcs) {
        atexit_funcs->func();
        atexit_funcs = atexit_funcs->next;
    }

    // TODO: Flush & close all streams

    hydrogen_exit();
}

EXPORT char *getenv(const char *name) {
    size_t i = 0;

    for (;;) {
        char *cur = environ[i++];
        if (!cur) return NULL;

        char *sep = __builtin_strchr(cur, '=');
        if (!sep) continue;
        size_t nlen = sep - cur;

        if (__builtin_strncmp(cur, name, nlen) == 0 && name[nlen] == 0) {
            return sep + 1;
        }
    }
}

EXPORT int system(const char *string) {
    // TODO

    if (!string) return 0; // No shell available

    errno = ENOSYS;
    return -1;
}

EXPORT void *bsearch(
        const void *key,
        const void *base,
        size_t nmemb,
        size_t size,
        int (*compar)(const void *, const void *)
) {
    size_t sub = 0;         // the index of the start of the subtree; the subtree's length is nmemb
    size_t idx = nmemb / 2; // the index within the subtree

    for (;;) {
        if (idx >= nmemb) return NULL;
        const void *cur = base + (sub + idx) * size;

        int cmp = compar(key, cur);
        if (cmp == 0) return (void *)cur;

        if (cmp < 0) {
            nmemb /= 2;
        } else {
            size_t extra = (nmemb + 1) / 2;
            sub += extra;
            nmemb -= extra;
        }

        idx = nmemb / 2;
    }
}

static void swap(void *base, size_t i1, size_t i2, size_t size) {
    unsigned char *b = base;

    i1 *= size;
    i2 *= size;

    for (size_t i = 0; i < size; i++) {
        unsigned char c1 = b[i1];
        b[i1] = b[i2];
        b[i2] = c1;
    }
}

// Heap sort (https://en.wikipedia.org/wiki/Heapsort#Standard_implementation)
EXPORT void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    size_t start = nmemb / 2;
    size_t end = nmemb;

    while (end > 1) {
        if (start > 0) {
            start -= 1;
        } else {
            end -= 1;
            swap(base, end, 0, size);
        }

        size_t root = start;
        size_t child = 2 * root + 1;

        while (child < end) {
            if (child + 1 < end && compar(base + child * size, base + (child + 1) * size) < 0) {
                child += 1;
            }

            if (compar(base + root * size, base + child * size) < 0) {
                swap(base, root, child, size);
                root = child;
                child = 2 * root + 1;
            } else {
                break;
            }
        }
    }
}

EXPORT int abs(int j) {
    return j >= 0 ? j : -j;
}

EXPORT div_t div(int numer, int denom) {
    return (div_t){numer / denom, numer % denom};
}

EXPORT long labs(long j) {
    return j >= 0 ? j : -j;
}

EXPORT ldiv_t ldiv(long numer, long denom) {
    return (ldiv_t){numer / denom, numer % denom};
}

EXPORT int mblen(const char *s, size_t n) {
    return mbtowc(NULL, s, n); // this is ok because mbtowc is stateless
}

EXPORT int mbtowc(wchar_t *pwc, const char *s, size_t n) {
    if (!s) return 0;
    if (n == 0) return -1;

    char c = *s;
    if (c == 0) {
        if (pwc) *pwc = 0;
        return 0;
    }

    size_t len;
    wchar_t val;
    wchar_t min;

    if ((c & 0x80) == 0) {
        if (pwc) *pwc = c;
        return 1;
    } else if ((c & 0xe0) == 0xc0) {
        len = 2;
        val = c & 0x1f;
        min = 0x80;
    } else if ((c & 0xf0) == 0xe0) {
        len = 3;
        val = c & 0x0f;
        min = 0x800;
    } else if ((c & 0xf8) == 0xf0) {
        len = 4;
        val = c & 0x07;
        min = 0x10000;
    } else {
        return -1;
    }

    if (len > n) return -1;

    while (len > 1) {
        c = *++s;
        if ((c & 0xc0) != 0x80) return -1;
        val = (val << 6) | (c & 0x3f);
    }

    return val >= min ? val : -1;
}

EXPORT int wctomb(char *s, wchar_t wchar) {
    if (!s) return 0;

    if (wchar < 0x80) {
        s[0] = wchar;
        return 1;
    } else if (wchar < 0x800) {
        s[0] = 0xc0 | (wchar >> 6);
        s[1] = 0x80 | (wchar & 0x3f);
        return 2;
    } else if (wchar < 0x10000) {
        s[0] = 0xe0 | (wchar >> 12);
        s[1] = 0x80 | ((wchar >> 6) & 0x3f);
        s[2] = 0x80 | (wchar & 0x3f);
        return 3;
    } else if (wchar < 0x110000) {
        s[0] = 0xf0 | (wchar >> 18);
        s[1] = 0x80 | ((wchar >> 12) & 0x3f);
        s[2] = 0x80 | ((wchar >> 6) & 0x3f);
        s[3] = 0x80 | (wchar & 0x3f);
        return 4;
    } else {
        return -1;
    }
}

EXPORT size_t mbstowcs(wchar_t *restrict pwcs, const char *restrict s, size_t n) {
    size_t cur = 0;

    while (n > 0) {
        int len = mbtowc(&pwcs[cur], s, n); // this is ok because mbtowc is stateless
        if (len < 0) return -1;
        if (len == 0) break;
        cur++;
        n -= 1;
    }

    return cur;
}

EXPORT size_t wcstombs(char *restrict s, const wchar_t *restrict pwcs, size_t n) {
    char buf[MB_CUR_MAX];
    size_t cur = 0;

    while (n > 0) {
        wchar_t c = *pwcs++;
        int len = wctomb(buf, c);
        if (len < 0) return -1;
        if ((size_t)len > n) break;
        __builtin_memcpy(&s[cur], buf, len);
        if (c == 0) break;
        cur += len;
        n -= len;
    }

    return cur;
}
