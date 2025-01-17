#ifndef _LIMITS_H
#define _LIMITS_H 1

#define LONG_MAX __LONG_MAX__
#define LONG_MIN (-LONG_MAX - 1)
#define ULONG_MAX (LONG_MAX * 2ul + 1ul)

#define INT_MAX __INT_MAX__
#define INT_MIN (-INT_MAX - 1)
#define UINT_MAX (INT_MAX * 2u + 1u)

#define CHAR_BIT __CHAR_BIT__
#define SCHAR_MAX __SCHAR_MAX__
#define SCHAR_MIN (-SCHAR_MAX - 1)
#if SCHAR_MAX == INT_MAX
#define UCHAR_MAX (SCHAR_MAX * 2u + 1u)
#else
#define UCHAR_MAX (SCHAR_MAX * 2 + 1)
#endif

#ifdef __CHAR_UNSIGNED__
#define CHAR_MAX UCHAR_MAX
#if SCHAR_MAX == INT_MAX
#define CHAR_MIN 0u
#else
#define CHAR_MIN 0
#endif
#else
#define CHAR_MAX SCHAR_MAX
#define CHAR_MIN SCHAR_MIN
#endif

#define MB_LEN_MAX 4

#define SHRT_MAX __SHRT_MAX__
#define SHRT_MIN (-SHRT_MAX - 1)

#if SHRT_MAX == INT_MAX
#define USHRT_MAX (SHRT_MAX * 2u + 1u)
#else
#define USHRT_MAX (SHRT_MAX * 2 + 1)
#endif

#endif /* _LIMITS_H */
