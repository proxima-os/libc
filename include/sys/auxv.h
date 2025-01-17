#ifndef _SYS_AUXV_H
#define _SYS_AUXV_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "elf.h" /* for AT_* */

unsigned long getauxval(unsigned long tag);

#ifdef __cplusplus
};
#endif

#endif /* _SYS_AUXV_H */
