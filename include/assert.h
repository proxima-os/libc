#ifndef _ASSERT_H
#define _ASSERT_H 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NDEBUG

__attribute__((__noreturn__)) void __assert_fail(const char *__expr, const char *__file, int __line);

#define assert(expr) ((expr) ? (void) 0 : __assert_fail(#expr, __FILE__, __LINE__))

#else
#define assert(ignore) ((void) 0)
#endif /* NDEBUG */

#ifdef __cplusplus
};
#endif

#endif /* _ASSERT_H */
