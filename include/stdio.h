#ifndef _STDIO_H
#define _STDIO_H 1

#define __need_va_list
#include <stdarg.h>

#define __need_NULL
#define __need_size_t
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned long __offset;
} fpos_t;

typedef struct {
    int __fd;
    int __flags;
    unsigned char __push_buffer[16];
    size_t __push_count;
} FILE;

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

#define BUFSIZ 0x2000

#define EOF (-1)

/* Completely arbitrary */
#define FOPEN_MAX 1024
#define FILENAME_MAX 256
#define L_tmpnam 32

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* The number of possible names that tmpnam can generate is 52 << (L_tmpnam-10), but that's too large for an int.
   0x200000 seems reasonable enough. */
#define TMP_MAX 0x200000

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;
#define stdin stdin
#define stdout stdout
#define stderr stderr

int remove(const char *__filename);
int rename(const char *__oldname, const char *__newname);
FILE *tmpfile(void);
char *tmpnam(char *__s);
int fclose(FILE *__stream);
int fflush(FILE *__stream);
FILE *fopen(const char *__filename, const char *__mode);
FILE *freopen(const char *__filename, const char *__mode, FILE *__stream);
void setbuf(FILE *__stream, char *__buf);
int setvbuf(FILE *__stream, char *__buf, int __mode, size_t __size);
__attribute__((__format__(__printf__, 2, 3))) int fprintf(FILE *__stream, const char *__format, ...);
__attribute__((__format__(__scanf__, 2, 3))) int fscanf(FILE *__stream, const char *__format, ...);
__attribute__((__format__(__printf__, 1, 2))) int printf(const char *__format, ...);
__attribute__((__format__(__scanf__, 1, 2))) int scanf(const char *__format, ...);
__attribute__((__format__(__printf__, 2, 3))) int sprintf(char *__s, const char *__format, ...);
__attribute__((__format__(__scanf__, 2, 3))) int sscanf(const char *__s, const char *__format, ...);
__attribute__((__format__(__printf__, 2, 0))) int vfprintf(FILE *__stream, const char *__format, va_list __arg);
__attribute__((__format__(__printf__, 1, 0))) int vprintf(const char *__format, va_list __arg);
__attribute__((__format__(__printf__, 2, 0))) int vsprintf(char *__s, const char *__format, va_list __arg);
int fgetc(FILE *__stream);
char *fgets(char *__s, int __n, FILE *__stream);
int fputc(int __c, FILE *__stream);
int fputs(const char *__s, FILE *__stream);
int getc(FILE *__stream);
int getchar(void);
char *gets(char *__s);
int putc(int __c, FILE *__stream);
int putchar(int __c);
int puts(const char *__s);
int ungetc(int __c, FILE *__stream);
size_t fread(void *__ptr, size_t __size, size_t __nmemb, FILE *__stream);
size_t fwrite(const void *__ptr, size_t __size, size_t __nmemb, FILE *__stream);
int fgetpos(FILE *__stream, fpos_t *__pos);
int fseek(FILE *__stream, long __offset, int __whence);
int fsetpos(FILE *__stream, const fpos_t *__pos);
long ftell(FILE *__stream);
void rewind(FILE *__stream);
void clearerr(FILE *__stream);
int feof(FILE *__stream);
int ferror(FILE *__stream);
void perror(const char *__s);

#ifdef __cplusplus
};
#endif

#endif /* _STDIO_H */
