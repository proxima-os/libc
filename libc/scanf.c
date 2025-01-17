#include "assert.h"
#include "compiler.h"
#include "ctype.h"
#include "limits.h"
#include "stdio.h"
#include "stdlib.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#define BUFFER_SIZE 256

typedef struct {
    int (*get)(void *ptr);
    void (*put)(int value, void *ptr); // `value` was returned by the previous call to `get` that hasn't been cancelled
} scanf_src_t;

typedef struct {
    scanf_src_t src;
    void *ptr;
    int count;
} scanf_ctx_t;

static int get(scanf_ctx_t *ctx) {
    int value = ctx->src.get(ctx->ptr);
    if (value >= 0) ctx->count += 1;
    return value;
}

static void put(scanf_ctx_t *ctx, int value) {
    assert(value >= 0);
    ctx->src.put(value, ctx->ptr);
    ctx->count -= 1;
}

static int skipws(scanf_ctx_t *ctx) {
    for (;;) {
        int value = get(ctx);
        if (value < 0 || !isspace(value)) return value;
    }
}

typedef enum {
    LEN_DEFAULT,
    LEN_SHORT,
    LEN_LONG,
    LEN_LONG_DOUBLE,
} length_modifier_t;

typedef enum {
    CONV_SDEC,
    CONV_SINT,
    CONV_UOCT,
    CONV_UDEC,
    CONV_UHEX,
    CONV_FLOAT,
    CONV_STRING,
    CONV_SCANSET,
    CONV_CHARS,
    CONV_POINTER,
    CONV_WRITEBACK,
    CONV_PERCENT,
} conv_spec_t;

typedef struct {
    bool no_assign;
    int width;
    length_modifier_t length;
    conv_spec_t conv;
} scanf_spec_t;

static int getdval(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c &= ~0x20;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return INT_MAX;
}

static int is_valid_digit(unsigned char c, int base) {
    if (base == 0) base = INT_MAX;
    return getdval(c) < base;
}

static int fill_int_buffer(scanf_ctx_t *ctx, scanf_spec_t *spec, char *buffer, size_t size, int base) {
    size_t index = 0;
    int max = spec->width ? spec->width : INT_MAX;

    int value = skipws(ctx);
    int ret = 1;

    if (value == '-' || value == '+') {
        buffer[index++] = value;
        value = get(ctx);
        ret = 0;
    }

    size_t start = index;

    for (;;) {
        if (value == EOF) break;

        if (((base != 0 && base != 16) || (index - start) != 1 || (value & ~0x20) != 'X') &&
            !is_valid_digit(value, base)) {
            put(ctx, value);
            break;
        }

        if (index != size - 1) buffer[index++] = value;

        ret = 0;
        if (--max == 0) break;

        value = get(ctx);
    }
    if (ret) return ret;

    buffer[index] = 0;
    return 0;
}

static int match_signed_int(scanf_ctx_t *ctx, scanf_spec_t *spec, intmax_t *out, int base) {
    char buffer[BUFFER_SIZE];
    int ret = fill_int_buffer(ctx, spec, buffer, sizeof(buffer), base);
    if (ret) return ret;

    char *end;
    long value = strtol(buffer, &end, base);
    if (out) *out = value;
    return *end != 0;
}

static int match_SDEC(scanf_ctx_t *ctx, scanf_spec_t *spec, intmax_t *out) {
    return match_signed_int(ctx, spec, out, 10);
}

static int match_SINT(scanf_ctx_t *ctx, scanf_spec_t *spec, intmax_t *out) {
    return match_signed_int(ctx, spec, out, 0);
}

static int match_unsigned_int(scanf_ctx_t *ctx, scanf_spec_t *spec, uintmax_t *out, int base) {
    char buffer[BUFFER_SIZE];
    int ret = fill_int_buffer(ctx, spec, buffer, sizeof(buffer), base);
    if (ret) return ret;

    char *end;
    unsigned long value = strtoul(buffer, &end, base);
    if (out) *out = value;
    return *end != 0;
}

static int match_UOCT(scanf_ctx_t *ctx, scanf_spec_t *spec, uintmax_t *out) {
    return match_unsigned_int(ctx, spec, out, 8);
}

static int match_UDEC(scanf_ctx_t *ctx, scanf_spec_t *spec, uintmax_t *out) {
    return match_unsigned_int(ctx, spec, out, 10);
}

static int match_UHEX(scanf_ctx_t *ctx, scanf_spec_t *spec, uintmax_t *out) {
    return match_unsigned_int(ctx, spec, out, 16);
}

typedef enum {
    FLOAT_INIT,
    FLOAT_WHOLE,
    FLOAT_FRAC,
    FLOAT_EXP_INIT,
    FLOAT_EXP_DIGITS,
} float_state_t;

static bool is_valid_float_char(float_state_t *state, unsigned char c) {
    float_state_t cur = *state;

    switch (cur) {
    case FLOAT_INIT:
        *state = FLOAT_WHOLE;
        if (c == '+' || c == '-') return true;
        // fall through
    case FLOAT_WHOLE:
        if (c == '.') {
            *state = FLOAT_FRAC;
            return true;
        }
        // fall through
    case FLOAT_FRAC:
        if ((c & ~0x20) == 'E') {
            *state = FLOAT_EXP_INIT;
            return true;
        }
        return c >= '0' && c <= '9';
    case FLOAT_EXP_INIT:
        *state = FLOAT_EXP_DIGITS;
        if (c == '+' || c == '-') return true;
        // fall through
    case FLOAT_EXP_DIGITS: return c >= '0' && c <= '9';
    default: __builtin_unreachable();
    }
}

static int fill_float_buffer(scanf_ctx_t *ctx, scanf_spec_t *spec, char *buffer, size_t size) {
    size_t index = 0;
    int max = spec->width ? spec->width : INT_MAX;

    int value = skipws(ctx);
    int ret = 1;
    float_state_t state = FLOAT_INIT;

    for (;;) {
        if (value == EOF) break;

        if (!is_valid_float_char(&state, value)) {
            put(ctx, value);
            break;
        }

        if (index != size - 1) buffer[index++] = value;

        ret = 0;
        if (--max == 0) break;

        value = get(ctx);
    }
    if (ret) return ret;

    buffer[index] = 0;
    return 0;
}

static int match_FLOAT(scanf_ctx_t *ctx, scanf_spec_t *spec, double *out) {
    char buffer[BUFFER_SIZE];
    int ret = fill_float_buffer(ctx, spec, buffer, sizeof(buffer));
    if (ret) return ret;

    char *end;
    double value = strtod(buffer, &end);
    if (out) *out = value;
    return *end != 0;
}

static int match_STRING(scanf_ctx_t *ctx, scanf_spec_t *spec, unsigned char *buffer) {
    int max = spec->width ? spec->width : INT_MAX;

    int value = skipws(ctx);
    int ret = 1;

    for (;;) {
        if (value == EOF) break;

        if (isspace(value)) {
            put(ctx, value);
            break;
        }

        *buffer++ = value;
        ret = 0;
        if (--max == 0) break;

        value = get(ctx);
    }

    *buffer = 0;
    return ret;
}

static int match_SCANSET(
        scanf_ctx_t *ctx,
        scanf_spec_t *spec,
        unsigned char *buffer,
        const void *set,
        size_t count,
        bool negate
) {
    unsigned char *start = buffer;
    int max = spec->width ? spec->width : INT_MAX;

    for (int i = 0; i < max; i++) {
        int value = get(ctx);
        if (value == EOF) break;

        if (!!__builtin_memchr(set, value, count) == !!negate) {
            put(ctx, value);
            break;
        }
        *buffer++ = value;
    }

    *buffer = 0;
    return start == buffer;
}

static int match_CHARS(scanf_ctx_t *ctx, scanf_spec_t *spec, unsigned char *buffer) {
    int count = spec->width ? spec->width : 1;

    int i;
    for (i = 0; i < count; i++) {
        int value = get(ctx);
        if (value == EOF) break;
        if (buffer) *buffer++ = value;
    }

    return i == 0;
}

static int match_POINTER(scanf_ctx_t *ctx, scanf_spec_t *spec, void **out) {
    uintmax_t value;
    int ret = match_UHEX(ctx, spec, &value);
    if (ret) return ret;

    *out = (void *)value;
    return 0;
}

static int parse_conv_spec(const char *format, scanf_spec_t *out) {
    const char *start = format;

    out->no_assign = format[1] == '*';
    if (out->no_assign) format++;

    out->width = 0;
    while (format[1] >= '0' && format[1] <= '9') out->width = (out->width * 10) + (*++format - '0');

    switch (*++format) {
    case 'h': out->length = LEN_SHORT; break;
    case 'l': out->length = LEN_LONG; break;
    case 'L': out->length = LEN_LONG_DOUBLE; break;
    default:
        out->length = LEN_DEFAULT;
        format--;
        break;
    }

    switch (*++format) {
    case 'd': out->conv = CONV_SDEC; break;
    case 'i': out->conv = CONV_SINT; break;
    case 'o': out->conv = CONV_UOCT; break;
    case 'u': out->conv = CONV_UDEC; break;
    case 'X':
    case 'x': out->conv = CONV_UHEX; break;
    case 'E':
    case 'e':
    case 'f':
    case 'G':
    case 'g': out->conv = CONV_FLOAT; break;
    case 's': out->conv = CONV_STRING; break;
    case '[': out->conv = CONV_SCANSET; break;
    case 'c': out->conv = CONV_CHARS; break;
    case 'p': out->conv = CONV_POINTER; break;
    case 'n': out->conv = CONV_WRITEBACK; break;
    case '%': out->conv = CONV_PERCENT; break;
    default: return 0;
    }

    return (format - start) + 1;
}

#define CONVERT(name, out)                                                                                             \
    do {                                                                                                               \
        int ret = match_##name(&ctx, &spec, out);                                                                      \
        if (ret == EOF) goto err;                                                                                      \
        if (ret) goto done;                                                                                            \
    } while (0)

#define BUFCONV(name, type)                                                                                            \
    CONVERT(name, !spec.no_assign ? va_arg(args, type) : NULL);                                                        \
    break

#define VALCONV(name, vtype, type)                                                                                     \
    if (!spec.no_assign) {                                                                                             \
        type value;                                                                                                    \
        CONVERT(name, &value);                                                                                         \
        *va_arg(args, vtype *) = (vtype)value;                                                                         \
    } else {                                                                                                           \
        CONVERT(name, NULL);                                                                                           \
    }                                                                                                                  \
    break

#define SINTCONV(name)                                                                                                 \
    switch (spec.length) {                                                                                             \
    case LEN_LONG_DOUBLE:                                                                                              \
    case LEN_DEFAULT: VALCONV(name, int, intmax_t);                                                                    \
    case LEN_SHORT: VALCONV(name, short, intmax_t);                                                                    \
    case LEN_LONG: VALCONV(name, long, intmax_t);                                                                      \
    }                                                                                                                  \
    break

#define UINTCONV(name)                                                                                                 \
    switch (spec.length) {                                                                                             \
    case LEN_LONG_DOUBLE:                                                                                              \
    case LEN_DEFAULT: VALCONV(name, unsigned, uintmax_t);                                                              \
    case LEN_SHORT: VALCONV(name, unsigned short, uintmax_t);                                                          \
    case LEN_LONG: VALCONV(name, unsigned long, uintmax_t);                                                            \
    }                                                                                                                  \
    break

static bool is_format_ws(char c) {
    return c == '\r' || c == '\n' || c == '\t' || c == ' ';
}

static int do_scanf(scanf_src_t src, void *ptr, const char *format, va_list args) {
    scanf_ctx_t ctx = {src, ptr, 0};
    int count = 0;
    int conversions = 0;

    for (unsigned char c = *format; c != 0; c = *++format) {
        if (c == '%') {
            scanf_spec_t spec;
            int length = parse_conv_spec(format, &spec);

            if (length != 0) {
                format += length - 1;

                switch (spec.conv) {
                case CONV_SDEC: SINTCONV(SDEC);
                case CONV_SINT: SINTCONV(SINT);
                case CONV_UOCT: UINTCONV(UOCT);
                case CONV_UDEC: UINTCONV(UDEC);
                case CONV_UHEX: UINTCONV(UHEX);
                case CONV_FLOAT:
                    switch (spec.length) {
                    case LEN_SHORT:
                    case LEN_DEFAULT: VALCONV(FLOAT, float, double);
                    case LEN_LONG: VALCONV(FLOAT, double, double);
                    case LEN_LONG_DOUBLE: VALCONV(FLOAT, long double, double);
                    }
                    break;
                case CONV_STRING: BUFCONV(STRING, unsigned char *);
                case CONV_SCANSET: {
                    bool negate = *++format == '^';
                    if (negate) format++;
                    const char *start = format;
                    do {
                        format++;
                    } while (*format != ']');

                    int ret = match_SCANSET(
                            &ctx,
                            &spec,
                            !spec.no_assign ? va_arg(args, unsigned char *) : NULL,
                            start,
                            format - start,
                            negate
                    );
                    if (ret == EOF) goto err;
                    if (ret) goto done;
                    break;
                }
                case CONV_CHARS: BUFCONV(CHARS, unsigned char *);
                case CONV_POINTER: BUFCONV(POINTER, void **);
                case CONV_WRITEBACK:
                    switch (spec.length) {
                    case LEN_LONG_DOUBLE:
                    case LEN_DEFAULT: *va_arg(args, int *) = ctx.count; break;
                    case LEN_SHORT: *va_arg(args, short *) = ctx.count; break;
                    case LEN_LONG: *va_arg(args, long *) = ctx.count; break;
                    }
                    continue; // skip incrementing count and conversions
                case CONV_PERCENT: {
                    int value = skipws(&ctx);
                    if (value == EOF) goto err;
                    if (value != '%') {
                        put(&ctx, value);
                        goto done;
                    }
                    break;
                }
                }

                if (!spec.no_assign) count += 1;
                conversions += 1;
                continue;
            }
        } else if (is_format_ws(c)) {
            int value = skipws(&ctx);
            if (value == EOF) goto err;
            put(&ctx, value);
            while (is_format_ws(format[1])) format++;
            continue;
        }

        int value = get(&ctx);
        if (value == EOF) goto err;
        if (value != c) {
            put(&ctx, value);
            break;
        }
    }

done:
    return count;
err:
    return conversions ? count : EOF;
}

static int scanf_file_get(void *ptr) {
    return fgetc(ptr);
}

static void scanf_file_put(int value, void *ptr) {
    UNUSED int ret = ungetc(value, ptr);
    assert(ret == value);
}

static int vfscanf(FILE *stream, const char *format, va_list args) {
    return do_scanf((scanf_src_t){scanf_file_get, scanf_file_put}, stream, format, args);
}

EXPORT int fscanf(FILE *stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vfscanf(stream, format, args);
    va_end(args);
    return ret;
}

EXPORT int scanf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vfscanf(stdin, format, args);
    va_end(args);
    return ret;
}

typedef struct {
    const char *cur;
} scanf_str_ctx_t;

static int scanf_str_get(void *ptr) {
    scanf_str_ctx_t *ctx = ptr;
    unsigned char c = *ctx->cur;

    if (c) {
        ctx->cur++;
        return c;
    } else {
        return EOF;
    }
}

static void scanf_str_put(UNUSED int value, void *ptr) {
    scanf_str_ctx_t *ctx = ptr;
    ctx->cur -= 1;
    assert((unsigned char)*ctx->cur == value);
}

EXPORT int sscanf(const char *s, const char *format, ...) {
    scanf_str_ctx_t ctx = {s};

    va_list args;
    va_start(args, format);
    int ret = do_scanf((scanf_src_t){scanf_str_get, scanf_str_put}, &ctx, format, args);
    va_end(args);

    return ret;
}
