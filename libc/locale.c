#include "locale.h"
#include "compiler.h"
#include <limits.h>
#include <stdio.h>

// TODO: Locale support

EXPORT char *setlocale(int category, const char *locale) {
    if (!locale) return "C";
    __builtin_fprintf(stderr, "libc: stub: setlocale(%d, \"%s\")\n", category, locale);
    return NULL;
}

EXPORT struct lconv *localeconv(void) {
    static const struct lconv c_lconv = {
            .decimal_point = ".",
            .thousands_sep = "",
            .grouping = "",
            .int_curr_symbol = "",
            .currency_symbol = "",
            .mon_decimal_point = "",
            .mon_thousands_sep = "",
            .mon_grouping = "",
            .positive_sign = "",
            .negative_sign = "",
            .int_frac_digits = CHAR_MAX,
            .frac_digits = CHAR_MAX,
            .p_cs_precedes = CHAR_MAX,
            .p_sep_by_space = CHAR_MAX,
            .n_cs_precedes = CHAR_MAX,
            .n_sep_by_space = CHAR_MAX,
            .p_sign_posn = CHAR_MAX,
            .n_sign_posn = CHAR_MAX,
    };

    return (struct lconv *)&c_lconv;
}
