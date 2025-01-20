#include "string.h"
#include "compiler.h"
#include "errno.h"
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

EXPORT char *strcpy(char *restrict s1, const char *restrict s2) {
    char *d = s1;

    for (;;) {
        char c = *s2++;
        *d++ = c;
        if (c == 0) break;
    }

    return s1;
}

EXPORT char *strncpy(char *restrict s1, const char *restrict s2, size_t n) {
    char *d = s1;

    while (n) {
        char c = *s2++;
        if (c == 0) break;
        *d++ = c;
        n--;
    }

    while (n--) {
        *d++ = 0;
    }

    return s1;
}

EXPORT char *strcat(char *restrict s1, const char *restrict s2) {
    __builtin_strcpy(s1 + __builtin_strlen(s1), s2);
    return s1;
}

EXPORT char *strncat(char *restrict s1, const char *restrict s2, size_t n) {
    __builtin_strncpy(s1 + __builtin_strlen(s1), s2, n + 1);
    return s1;
}

EXPORT int strcoll(const char *s1, const char *s2) {
    return __builtin_strcmp(s1, s2); // TODO: Locale support
}

EXPORT int strncmp(const char *s1, const char *s2, size_t n) {
    while (n--) {
        unsigned char c1 = *s1++;
        unsigned char c2 = *s2++;

        if (c1 < c2) return -1;
        if (c1 > c2) return 1;
        if (c1 == 0) return 0;
    }

    return 0;
}

EXPORT size_t strxfrm(char *restrict s1, const char *restrict s2, size_t n) {
    size_t len = 0;

    for (;;) {
        char c = *s2++;

        if (n) {
            *s1++ = c;
            n--;
        }

        if (!c) break;
        len++;
    }

    return len;
}

EXPORT void *memchr(const void *s, int c, size_t n) {
    const unsigned char *str = s;
    unsigned char val = c;

    while (n--) {
        unsigned char cur = *str;
        if (cur == val) return (void *)str;
        str++;
    }

    return NULL;
}

EXPORT char *strchr(const char *s, int c) {
    char val = c;

    for (char cur = *s; cur != 0; cur = *++s) {
        if (cur == val) return (char *)s;
    }

    return NULL;
}

EXPORT size_t strcspn(const char *s1, const char *s2) {
    size_t len = 0;

    for (;;) {
        char c1 = *s1++;
        char c2 = *s2++;
        if (c1 == c2) return len;
        if (!(c1 | c2)) return len;
        len++;
    }
}

EXPORT char *strpbrk(const char *s1, const char *s2) {
    for (char c = *s1; c != 0; c = *++s1) {
        if (__builtin_strchr(s2, c)) return (char *)s1;
    }

    return NULL;
}

EXPORT char *strrchr(const char *s1, int c) {
    char *last = NULL;
    char val = c;

    for (char cur = *s1; cur != 0; cur = *s1++) {
        if (cur == val) last = (char *)s1;
    }

    return last;
}

EXPORT size_t strspn(const char *s1, const char *s2) {
    size_t len = 0;

    for (;;) {
        char c1 = *s1++;
        char c2 = *s2++;
        if (c1 != c2) return len;
        if (!c1) return len;
        len++;
    }
}

EXPORT char *strstr(const char *s1, const char *s2) {
    for (;;) {
        const char *cs1 = s1;
        const char *cs2 = s2;

        for (;;) {
            char c2 = *cs2++;
            if (!c2) return (char *)s1;

            char c1 = *cs1++;
            if (!c1) return NULL;
            if (c1 != c2) break;
        }

        s1++;
    }
}

EXPORT char *strtok(char *restrict s1, const char *restrict s2) {
    static char *state;
    char *cur = s1 ? s1 : state;

    // Find starting position
    for (;;) {
        char c = *cur;

        if (!c) {
            state = cur;
            return NULL;
        }

        if (!__builtin_strchr(s2, c)) {
            break;
        }

        cur++;
    }

    // Find end of token
    char *end = cur;
    for (char c = *end; c != 0; c = *++end) {
        if (__builtin_strchr(s2, c)) {
            state = end + 1;
            *end = 0;
            return cur;
        }

        end++;
    }

    state = end;
    return cur;
}

EXPORT char *strerror(int errnum) {
    switch (errnum) {
    case 0: return "Success";
    case __EACCES: return "Access denied";
    case __EBADF: return "Bad file descriptor";
    case __EBUSY: return "Device or resource busy";
    case __EDOM: return "Numerical argument out of domain";
    case __EEXIST: return "Already exists";
    case __EFAULT: return "Invalid address";
    case __EILSEQ: return "Illegal byte sequence";
    case __EINVAL: return "Invalid argument";
    case __EISDIR: return "Is a directory";
    case __ELOOP: return "Too many levels of symbolic links";
    case __EMFILE: return "Too many open files";
    case __ENAMETOOLONG: return "Filename too long";
    case __ENOENT: return "No such file or directory";
    case __ENOEXEC: return "Executable file format error";
    case __ENOSPC: return "Disk full";
    case __ENOSYS: return "Not implemented";
    case __ENOTDIR: return "Not a directory";
    case __ENOTEMPTY: return "Directory not empty";
    case __EOVERFLOW: return "Value too large for defined data type";
    case __ERANGE: return "Numerical result out of range";
    case __EXDEV: return "Invalid cross-device link";
    default: errno = __EINVAL; return "Unknown error";
    }
}

EXPORT size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}
