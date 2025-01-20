#ifndef _ERRNO_H
#define _ERRNO_H 1

#include <hydrogen/error.h>

#ifdef __cplusplus
extern "C" {
#endif

#define __EACCES ERR_ACCESS_DENIED
#define __EBADF ERR_INVALID_HANDLE
#define __EBUSY ERR_BUSY
#define __EDOM 0x40000000
#define __EEXIST ERR_ALREADY_EXISTS
#define __EFAULT ERR_INVALID_POINTER
#define __EILSEQ 0x40000002
#define __EINVAL ERR_INVALID_ARGUMENT
#define __EISDIR ERR_IS_A_DIRECTORY
#define __ELOOP ERR_TOO_MANY_SYMLINKS
#define __EMFILE ERR_NO_MORE_HANDLES
#define __ENAMETOOLONG ERR_NAME_TOO_LONG
#define __ENOENT ERR_NOT_FOUND
#define __ENOEXEC ERR_INVALID_IMAGE
#define __ENOMEM ERR_OUT_OF_MEMORY
#define __ENOSPC ERR_DISK_FULL
#define __ENOSYS ERR_NOT_IMPLEMENTED
#define __ENOTDIR ERR_NOT_A_DIRECTORY
#define __ENOTEMPTY ERR_NOT_EMPTY
#define __EOVERFLOW ERR_OVERFLOW
#define __ERANGE 0x40000001
#define __EXDEV ERR_DIFFERENT_FILESYSTEMS

#define EDOM __EDOM
#define ERANGE __ERANGE

#if __STDC_VERSION__ >= 199409L
#define EILSEQ __EILSEQ
#endif

extern int __errno;
#define errno __errno

#ifdef __cplusplus
};
#endif

#endif /* _ERRNO_H */
