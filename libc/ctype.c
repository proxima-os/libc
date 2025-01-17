#include "ctype.h"
#include "compiler.h"

// TODO: Locale support

EXPORT int isalnum(int c) {
    return isalpha(c) || isdigit(c);
}

EXPORT int isalpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

EXPORT int iscntrl(int c) {
    return (c >= 0 && c <= 0x1f) || c == 0x7f;
}

EXPORT int isdigit(int c) {
    return c >= '0' && c <= '9';
}

EXPORT int isgraph(int c) {
    return c > ' ' && c <= '~';
}

EXPORT int islower(int c) {
    return c >= 'a' && c <= 'z';
}

EXPORT int isprint(int c) {
    return c >= ' ' && c <= '~';
}

EXPORT int ispunct(int c) {
    return isgraph(c) && !isalnum(c);
}

EXPORT int isspace(int c) {
    return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}

EXPORT int isupper(int c) {
    return c >= 'A' && c <= 'Z';
}

EXPORT int isxdigit(int c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

EXPORT int tolower(int c) {
    return isupper(c) ? c - 0x20 : c;
}

EXPORT int toupper(int c) {
    return islower(c) ? c + 0x20 : c;
}
