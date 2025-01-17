#include "assert.h"
#include "compiler.h"
#include "ryu/ryu_low_level.h"
#include "stdio.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

typedef bool (*printf_sink_t)(const void *data, size_t size, void *ptr);

typedef struct {
    printf_sink_t sink;
    void *ptr;
    int count;
} printf_ctx_t;

typedef enum {
    LEN_DEFAULT,
    LEN_SHORT,
    LEN_LONG,
    LEN_LONG_DOUBLE,
} printf_length_t;

typedef enum {
    CONV_SINT,
    CONV_UINT,
    CONV_OCT,
    CONV_HEX,
    CONV_FLOAT_DEC,
    CONV_FLOAT_SCI,
    CONV_FLOAT_AUTO,
    CONV_CHAR,
    CONV_STRING,
    CONV_POINTER,
    CONV_WRITEBACK,
    CONV_PERCENT,
} printf_conv_t;

typedef struct {
    bool left_justified;
    char positive_sign;
    bool alternate;
    bool pad_zero;
    char case_adjust;
    int field_width;
    int precision;
    printf_length_t length;
    printf_conv_t conv;
} printf_spec_t;

static bool do_print(printf_ctx_t *ctx, const void *data, size_t size) {
    ctx->count += size;
    return !size || ctx->sink(data, size, ctx->ptr);
}

static bool print_chars(printf_ctx_t *ctx, char c, size_t count) {
    while (count--) {
        if (!do_print(ctx, &c, sizeof(c))) return false;
    }

    return true;
}

#define PAD(char, count)                                                                                               \
    do {                                                                                                               \
        if (!print_chars(ctx, (char), (count))) return false;                                                          \
    } while (0)

#define PRINT(data, size)                                                                                              \
    do {                                                                                                               \
        if (!do_print(ctx, (data), (size))) return false;                                                              \
    } while (0)

#define PRINT_FUNC(name, type) static bool print_##name(printf_ctx_t *ctx, printf_spec_t *spec, type value)

#define PREPARE(length)                                                                                                \
    do {                                                                                                               \
        size_t _len = (length);                                                                                        \
        if (!spec->left_justified && _len < (size_t)spec->field_width) PAD(' ', spec->field_width - _len);             \
    } while (0)

#define FINALIZE(length)                                                                                               \
    do {                                                                                                               \
        size_t _len = (length);                                                                                        \
        if (spec->left_justified && _len < (size_t)spec->field_width) PAD(' ', spec->field_width - _len);              \
    } while (0)

static size_t print_into_buffer(unsigned char *buffer, size_t len, uintmax_t value, unsigned base, char case_adjust) {
    size_t index = len;

    while (value) {
        unsigned dval = value % base;
        value /= base;
        buffer[--index] = (dval < 10 ? '0' : 'a' - 10 + case_adjust) + dval;
    }

    return index;
}

static bool print_int(printf_ctx_t *ctx, printf_spec_t *spec, uintmax_t value, unsigned base, char sign, char prefix) {
    if (spec->precision < 0) spec->precision = 1;

    unsigned char buffer[64];
    size_t index = print_into_buffer(buffer, sizeof(buffer), value, base, spec->case_adjust);
    size_t raw_len = sizeof(buffer) - index;
    size_t pad_len = raw_len < (size_t)spec->precision ? spec->precision - raw_len : (prefix == '0');
    size_t length = raw_len + pad_len;

    if (prefix != 0 && prefix != '0' && raw_len != 0) {
        length += 2;
    }

    length += !!sign;

    size_t fpad_len;
    if (length < (size_t)spec->field_width) {
        fpad_len = spec->field_width - length;

        if (spec->pad_zero) {
            pad_len += fpad_len;
            fpad_len = 0;
        } else if (!spec->left_justified) {
            PAD(' ', fpad_len);
            fpad_len = 0;
        }
    } else {
        fpad_len = 0;
    }

    if (sign) PRINT(&sign, sizeof(sign));

    if (prefix != 0 && prefix != '0' && raw_len != 0) {
        unsigned char buf[2] = {'0', prefix + spec->case_adjust};
        PRINT(buf, sizeof(buf));
    }

    PAD('0', pad_len);
    PRINT(&buffer[index], raw_len);
    PAD(' ', fpad_len);

    return true;
}

PRINT_FUNC(SINT, intmax_t) {
    char sign;
    if (value >= 0) {
        sign = spec->positive_sign;
    } else {
        sign = '-';
        value = -value;
    }

    return print_int(ctx, spec, value, 10, sign, 0);
}

PRINT_FUNC(UINT, uintmax_t) {
    return print_int(ctx, spec, value, 10, 0, 0);
}

PRINT_FUNC(OCT, uintmax_t) {
    return print_int(ctx, spec, value, 8, 0, spec->alternate ? '0' : 0);
}

PRINT_FUNC(HEX, uintmax_t) {
    return print_int(ctx, spec, value, 16, 0, spec->alternate ? 'x' : 0);
}

_Static_assert(sizeof(uint64_t) >= sizeof(double), "uint64_t is smaller than double");

#define MANTISSA_BITS 52
#define EXPONENT_BITS 11

static uint64_t get_fp_bits(double value) {
    union {
        double value;
        uint64_t bits;
    } fp;
    fp.value = value;
    return fp.bits;
}

static struct floating_decimal convert_to_decimal(double value) {
    return generic_binary_to_decimal(get_fp_bits(value), MANTISSA_BITS, EXPONENT_BITS);
}

static const uint64_t iexp10[20] = {
        1ul,
        10ul,
        100ul,
        1000ul,
        10000ul,
        100000ul,
        1000000ul,
        10000000ul,
        100000000ul,
        1000000000ul,
        10000000000ul,
        100000000000ul,
        1000000000000ul,
        10000000000000ul,
        100000000000000ul,
        1000000000000000ul,
        10000000000000000ul,
        100000000000000000ul,
        1000000000000000000ul,
        10000000000000000000ul,
};

#define MAX_DIGITS (sizeof(iexp10) / sizeof(*iexp10))

static unsigned count_digits(uint64_t value) {
    for (size_t i = MAX_DIGITS - 1; i > 0; i--) {
        if (value >= iexp10[i]) return i + 1;
    }

    return 1;
}

static char get_sign(printf_spec_t *spec, struct floating_decimal value) {
    if (value.sign) {
        return '-';
    } else {
        return spec->positive_sign;
    }
}

static bool do_float_dec(printf_ctx_t *ctx, printf_spec_t *spec, struct floating_decimal value, bool remove_trailing) {
    char sign = get_sign(spec, value);
    const char *fixed;

    switch (value.type) {
    case RYU_NAN: fixed = spec->case_adjust ? "NAN" : "nan"; goto fixed;
    case RYU_INFINITY: fixed = spec->case_adjust ? "INF" : "inf"; goto fixed;
    case RYU_VALUE: break;
    }

    int prec;
    if (spec->precision >= 0) prec = spec->precision;
    else prec = 6;

    // round mantissa if necessary
    if (value.exponent < 0) {
        int fract_digits = -value.exponent;
        int extra_digits = fract_digits - prec;

        if (extra_digits > 0) {
            if (extra_digits < (int)MAX_DIGITS) {
                uint64_t divisor = iexp10[extra_digits];
                value.mantissa = (value.mantissa + (divisor / 2)) / divisor;
            } else {
                value.mantissa = 0;
            }

            value.exponent += extra_digits;
        }
    }

    // print the mantissa as an integer
    unsigned char mantissa_buf[MAX_DIGITS];
    size_t mantissa_idx = print_into_buffer(mantissa_buf, sizeof(mantissa_buf), value.mantissa, 10, 0);
    size_t mantissa_len = sizeof(mantissa_buf) - mantissa_idx;

    // determine location of decimal point within mantissa buffer, as well as the necessary padding zeroes

    size_t pre_dec_zeroes;
    size_t decimal_index;
    size_t post_dec_zeroes;
    size_t post_frac_zeroes;

    if (value.exponent >= 0) {
        pre_dec_zeroes = value.exponent;
        decimal_index = mantissa_len;
        post_dec_zeroes = 0;
        post_frac_zeroes = !remove_trailing ? prec : 0;
    } else {
        int frac_len = -value.exponent;

        if (frac_len < (int)mantissa_len) {
            pre_dec_zeroes = 0;
            decimal_index = mantissa_len - frac_len;
            post_dec_zeroes = 0;
        } else if (frac_len == (int)mantissa_len) {
            pre_dec_zeroes = 1;
            decimal_index = 0;
            post_dec_zeroes = 0;
        } else {
            pre_dec_zeroes = 1;
            decimal_index = 0;
            post_dec_zeroes = frac_len - mantissa_len;
        }

        int extra_digits = frac_len - prec;

        if (extra_digits < 0) {
            post_frac_zeroes = prec - frac_len;
        } else {
            if (extra_digits > 0) {
                if (mantissa_len >= (size_t)extra_digits) {
                    mantissa_len -= extra_digits;
                } else {
                    post_dec_zeroes -= extra_digits - mantissa_len;
                    mantissa_len = 0;
                }
            }

            post_frac_zeroes = 0;
        }

        if (remove_trailing) {
            while (decimal_index < mantissa_len && mantissa_buf[mantissa_idx + mantissa_len - 1] == '0') {
                mantissa_len -= 1;
            }

            if (decimal_index == mantissa_idx) post_dec_zeroes = 0;
            post_frac_zeroes = 0;
        }
    }

    size_t whole_digits = decimal_index + pre_dec_zeroes;
    size_t real_frac_digits = mantissa_len - decimal_index;
    size_t frac_digits = post_dec_zeroes + real_frac_digits + post_frac_zeroes;

    // determine how many padding chars are needed to fill the field, and pad if right-justified without zero-padding

    size_t whole_length = !!sign + whole_digits;
    size_t frac_length = (frac_digits || spec->alternate) + frac_digits;
    size_t field_length = whole_length + frac_length;
    size_t pad_len;

    if (field_length < (size_t)spec->field_width) {
        pad_len = spec->field_width;

        if (!spec->pad_zero && !spec->left_justified) {
            PAD(' ', pad_len);
            pad_len = 0;
        }
    } else {
        pad_len = 0;
    }

    // print sign, pad if zero-padded, and print part of mantissa before decimal point
    if (sign) PRINT(&sign, sizeof(sign));
    if (spec->pad_zero) {
        PAD('0', pad_len);
        pad_len = 0;
    }
    PRINT(&mantissa_buf[mantissa_idx], decimal_index);
    PAD('0', pre_dec_zeroes);

    // print the fractional part
    if (frac_length) {
        char c = '.';
        PRINT(&c, sizeof(c));
        PAD('0', post_dec_zeroes);
        PRINT(&mantissa_buf[mantissa_idx + decimal_index], real_frac_digits);
        PAD('0', post_frac_zeroes);
    }

    // pad if left-justified
    PAD(' ', pad_len);

    return true;

fixed: {
    size_t len = !!sign + 3;

    PREPARE(len);
    if (sign) PRINT(&sign, sizeof(sign));
    PRINT(fixed, 3);
    FINALIZE(len);

    return true;
}
}

static int get_sci_exponent(struct floating_decimal value, int *frac_digits_out) {
    int frac_digits = count_digits(value.mantissa) - 1;
    if (frac_digits_out) *frac_digits_out = frac_digits;
    return value.exponent + frac_digits;
}

static bool do_float_sci(printf_ctx_t *ctx, printf_spec_t *spec, struct floating_decimal value, bool remove_trailing) {
    size_t prec;
    if (spec->precision >= 0) prec = spec->precision;
    else prec = 6;

    // get the real exponent and round the mantissa if necessary
    int frac_len;
    int exponent = get_sci_exponent(value, &frac_len);
    int extra_digits = frac_len - prec;
    if (extra_digits > 0) {
        uint64_t divisor = iexp10[extra_digits];
        value.mantissa = (value.mantissa + (divisor / 2)) / divisor;
    }

    // print the components as integers
    bool exponent_neg = exponent < 0;
    if (exponent_neg) exponent = -exponent;

    unsigned char mantissa_buf[MAX_DIGITS];
    unsigned char exponent_buf[MAX_DIGITS + 2];
    size_t mantissa_idx = print_into_buffer(mantissa_buf, sizeof(mantissa_buf), value.mantissa, 10, 0);
    size_t exponent_idx = print_into_buffer(exponent_buf, sizeof(exponent_buf), exponent, 10, 0);
    size_t mantissa_len = sizeof(mantissa_buf) - mantissa_idx;
    size_t exponent_len = sizeof(exponent_buf) - exponent_idx;

    // make sure the buffers are formatted correctly

    if (mantissa_len == 0) {
        mantissa_buf[--mantissa_idx] = '0';
        mantissa_len = 1;
    }

    while (exponent_len < 2) {
        exponent_buf[--exponent_idx] = '0';
        exponent_len += 1;
    }

    exponent_buf[--exponent_idx] = "+-"[exponent_neg];
    exponent_buf[--exponent_idx] = 'e' + spec->case_adjust;
    exponent_len += 2;

    // calculate the total printed length without padding (except for the digits printed for precision)

    size_t real_frac_digits = mantissa_len - 1;
    size_t post_frac_zeroes;

    if (remove_trailing) {
        post_frac_zeroes = 0;

        while (real_frac_digits > 0 && mantissa_buf[mantissa_idx + real_frac_digits] == '0') {
            real_frac_digits -= 1;
        }
    } else if (real_frac_digits < prec) {
        post_frac_zeroes = prec - real_frac_digits;
    } else {
        // due to the rounding earlier, it shouldn't be possible for the fractional length to exceed the precision
        assert(real_frac_digits == prec);
        post_frac_zeroes = 0;
    }

    size_t frac_digits = real_frac_digits + post_frac_zeroes;
    size_t frac_length = frac_digits + (frac_digits || spec->alternate); // \.[0-9]*

    char sign = get_sign(spec, value);
    size_t whole_len = !!sign + 1; // [+- ]?[0-9]

    size_t field_length = whole_len + frac_length + exponent_len;
    size_t pad_len;

    // determine how many padding chars are needed to fill the field, and pad if right-justified without zero-padding

    if (field_length < (size_t)spec->field_width) {
        pad_len = spec->field_width - field_length;

        if (!spec->pad_zero && !spec->left_justified) {
            PAD(' ', pad_len);
            pad_len = 0;
        }
    } else {
        pad_len = 0;
    }

    // print sign, pad if zero-padded, and print the digit before the decimal point
    if (sign) PRINT(&sign, sizeof(sign));
    if (spec->pad_zero) {
        PAD('0', pad_len);
        pad_len = 0;
    }
    PRINT(&mantissa_buf[mantissa_idx], 1);

    // print the fractional part
    if (frac_length) {
        char c = '.';
        PRINT(&c, sizeof(c));
        PRINT(&mantissa_buf[mantissa_idx + 1], real_frac_digits);
        PAD('0', post_frac_zeroes);
    }

    // print the exponent and pad if left-justified
    PRINT(&exponent_buf[exponent_idx], exponent_len);
    PAD(' ', pad_len);

    return true;
}

PRINT_FUNC(FLOAT_DEC, double) {
    return do_float_dec(ctx, spec, convert_to_decimal(value), false);
}

PRINT_FUNC(FLOAT_SCI, double) {
    struct floating_decimal v = convert_to_decimal(value);

    return v.type == RYU_VALUE ? do_float_sci(ctx, spec, v, false) : do_float_dec(ctx, spec, v, false);
}

PRINT_FUNC(FLOAT_AUTO, double) {
    int prec = spec->precision;
    if (prec < 0) prec = 6;
    else if (prec == 0) prec = 1;

    struct floating_decimal v = convert_to_decimal(value);

    if (v.type == RYU_VALUE) {
        int exp = get_sci_exponent(v, NULL);

        if (exp < -4 || exp >= prec) {
            spec->precision = prec - 1;
            return do_float_sci(ctx, spec, v, !spec->alternate);
        }

        spec->precision = prec - (exp + 1);
    }

    return do_float_dec(ctx, spec, v, !spec->alternate);
}

PRINT_FUNC(CHAR, char) {
    PREPARE(sizeof(value));
    PRINT(&value, sizeof(value));
    FINALIZE(sizeof(value));

    return true;
}

PRINT_FUNC(STRING, const char *) {
    if (!value) value = "(null)";

    size_t len = __builtin_strlen(value);
    if (spec->precision >= 0 && (size_t)spec->precision < len) len = spec->precision;

    PREPARE(len);
    PRINT(value, len);
    FINALIZE(len);

    return true;
}

PRINT_FUNC(POINTER, const void *) {
    spec->alternate = '#';
    spec->case_adjust = 0;

    return print_HEX(ctx, spec, (uintptr_t)value);
}

#undef PAD
#undef PRINT
#undef PRINT_FUNC

#define OPT_ARG -2

static int parse_specifier(const char *format, printf_spec_t *out) {
    const char *start = format;

    out->left_justified = false;
    out->positive_sign = 0;
    out->alternate = false;
    out->pad_zero = false;
    out->case_adjust = 0;
    out->field_width = 0;
    out->precision = -1;
    out->length = LEN_DEFAULT;

    char c;

    for (;;) {
        c = *++format;

        switch (c) {
        case '-':
            out->left_justified = true;
            out->pad_zero = false;
            continue;
        case '+': out->positive_sign = '+'; continue;
        case ' ':
            if (!out->positive_sign) out->positive_sign = ' ';
            continue;
        case '#': out->alternate = true; continue;
        case '0':
            if (!out->left_justified) out->pad_zero = true;
            continue;
        }

        break;
    }

    if (c == '*') {
        out->field_width = OPT_ARG;
        c = *++format;
    } else {
        while (c >= '0' && c <= '9') {
            out->field_width = (out->field_width * 10) + (c - '0');
            c = *++format;
        }
    }

    if (c == '.') {
        c = *++format;

        if (c == '*') {
            out->precision = OPT_ARG;
        } else {
            out->precision = 0;

            while (c >= '0' && c <= '9') {
                out->precision = (out->precision * 10) + (c - '0');
                c = *++format;
            }
        }
    }

    format++;
    switch (c) {
    case 'h': out->length = LEN_SHORT; break;
    case 'l': out->length = LEN_LONG; break;
    case 'L': out->length = LEN_LONG_DOUBLE; break;
    default: format--; break;
    }

    switch (*format++) {
    case 'd':
    case 'i': out->conv = CONV_SINT; break;
    case 'o': out->conv = CONV_OCT; break;
    case 'u': out->conv = CONV_UINT; break;
    case 'X': out->case_adjust = 'A' - 'a'; // fall through
    case 'x': out->conv = CONV_HEX; break;
    case 'f': out->conv = CONV_FLOAT_DEC; break;
    case 'E': out->case_adjust = 'A' - 'a'; // fall through
    case 'e': out->conv = CONV_FLOAT_SCI; break;
    case 'G': out->case_adjust = 'G' - 'g'; // fall through
    case 'g': out->conv = CONV_FLOAT_AUTO; break;
    case 'c': out->conv = CONV_CHAR; break;
    case 's': out->conv = CONV_STRING; break;
    case 'p': out->conv = CONV_POINTER; break;
    case 'n': out->conv = CONV_WRITEBACK; break;
    case '%': out->conv = CONV_PERCENT; break;
    default: return 0;
    }

    return format - start;
}

#define EXTRACT(func, len, type, vtype)                                                                                \
    case LEN_##len: r = print_##func(&ctx, &spec, (type)va_arg(args, vtype)); break

#define UINTCONV(type)                                                                                                 \
    case CONV_##type:                                                                                                  \
        switch (spec.length) {                                                                                         \
        case LEN_LONG_DOUBLE:                                                                                          \
            EXTRACT(type, DEFAULT, unsigned, unsigned);                                                                \
            EXTRACT(type, SHORT, unsigned short, unsigned);                                                            \
            EXTRACT(type, LONG, unsigned long, unsigned long);                                                         \
        }                                                                                                              \
        break

#define FLTCONV(type)                                                                                                  \
    case CONV_FLOAT_##type:                                                                                            \
        switch (spec.length) {                                                                                         \
        case LEN_SHORT:                                                                                                \
        case LEN_LONG:                                                                                                 \
            EXTRACT(FLOAT_##type, DEFAULT, double, double);                                                            \
            EXTRACT(FLOAT_##type, LONG_DOUBLE, double, long double);                                                   \
        }                                                                                                              \
        break

static int do_printf(printf_sink_t sink, void *ptr, const char *format, va_list args) {
    printf_ctx_t ctx = {sink, ptr, 0};
    const char *last = format;

    for (char c = *format; c != 0; c = *++format) {
        if (c != '%') continue;

        printf_spec_t spec;
        int length = parse_specifier(format, &spec);
        if (!length) continue;
        if (last != format && !do_print(&ctx, last, format - last)) return -1;
        last = format + length;
        format = last - 1;

        if (spec.field_width == OPT_ARG) {
            spec.field_width = va_arg(args, int);
            if (spec.field_width < 0) {
                spec.field_width = -spec.field_width;
                spec.left_justified = true;
            }
        }

        if (spec.precision == OPT_ARG) {
            spec.precision = va_arg(args, int);
        }

        bool r;
        switch (spec.conv) {
        case CONV_SINT:
            switch (spec.length) {
            case LEN_LONG_DOUBLE:
            case LEN_DEFAULT: r = print_SINT(&ctx, &spec, va_arg(args, int)); break;
            case LEN_SHORT: r = print_SINT(&ctx, &spec, (short)va_arg(args, int)); break;
            case LEN_LONG: r = print_SINT(&ctx, &spec, va_arg(args, long)); break;
            }
            break;
            UINTCONV(UINT);
            UINTCONV(OCT);
            UINTCONV(HEX);
            FLTCONV(DEC);
            FLTCONV(SCI);
            FLTCONV(AUTO);
        case CONV_CHAR: r = print_CHAR(&ctx, &spec, va_arg(args, int)); break;
        case CONV_STRING: r = print_STRING(&ctx, &spec, va_arg(args, const char *)); break;
        case CONV_POINTER: r = print_POINTER(&ctx, &spec, va_arg(args, void *)); break;
        case CONV_WRITEBACK:
            r = true;
            switch (spec.length) {
            case LEN_LONG_DOUBLE:
            case LEN_DEFAULT: *va_arg(args, int *) = ctx.count; break;
            case LEN_SHORT: *va_arg(args, short *) = ctx.count; break;
            case LEN_LONG: *va_arg(args, long *) = ctx.count; break;
            }
            break;
        case CONV_PERCENT: r = print_CHAR(&ctx, &spec, '%'); break;
        }
        if (!r) return -1;
    }

    if (last != format && !do_print(&ctx, last, format - last)) return -1;
    return ctx.count;
}

static bool file_sink(const void *data, size_t size, void *ctx) {
    return fwrite(data, 1, size, ctx) == size;
}

EXPORT int fprintf(FILE *stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = do_printf(file_sink, stream, format, args);
    va_end(args);
    return ret;
}

EXPORT int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = do_printf(file_sink, stdout, format, args);
    va_end(args);
    return ret;
}

static bool string_sink(const void *data, size_t size, void *ctx) {
    char **ptr = ctx;
    __builtin_memcpy(*ptr, data, size);
    *ptr += size;
    return true;
}

EXPORT int sprintf(char *s, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = do_printf(string_sink, &s, format, args);
    va_end(args);
    if (ret >= 0) *s = 0;
    return ret;
}

EXPORT int vfprintf(FILE *stream, const char *format, va_list arg) {
    return do_printf(file_sink, stream, format, arg);
}

EXPORT int vprintf(const char *format, va_list arg) {
    return do_printf(file_sink, stdout, format, arg);
}

EXPORT int vsprintf(char *s, const char *format, va_list arg) {
    int ret = do_printf(string_sink, s, format, arg);
    if (ret >= 0) *s = 0;
    return ret;
}
