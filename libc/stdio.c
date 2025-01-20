#include "stdio.h"
#include "assert.h"
#include "compiler.h"
#include "errno.h"
#include "limits.h"
#include "stdlib.h"
#include "string.h"
#include <hydrogen/fcntl.h>
#include <hydrogen/vfs.h>

#define STREAM_EOF (1 << 0)
#define STREAM_ERR (1 << 1)

EXPORT FILE *stdin;
EXPORT FILE *stdout;
EXPORT FILE *stderr;

EXPORT int remove(const char *filename) {
    int error = hydrogen_unlink(-1, filename, __builtin_strlen(filename), false);
    if (error) {
        errno = error;
        return 1;
    }

    return 0;
}

EXPORT int rename(const char *oldname, const char *newname) {
    int error = hydrogen_rename(-1, oldname, __builtin_strlen(oldname), -1, newname, __builtin_strlen(newname));
    if (error) {
        errno = error;
        return 1;
    }

    return 0;
}

static void openfd(FILE *stream, int fd) {
    __builtin_memset(stream, 0, sizeof(*stream));
    stream->__fd = fd;
}

static void generate_temp_name(char *buffer) {
    __builtin_memcpy(buffer, "/tmp/tmp.", 9);

    for (int i = 9; i < L_tmpnam - 1; i++) {
        int j = rand() % 52;
        char c = j < 26 ? 'a' + j : 'A' + (j - 26);
        buffer[i] = c;
    }

    buffer[L_tmpnam - 1] = 0;
}

EXPORT FILE *tmpfile(void) {
    char buffer[L_tmpnam];

    FILE *stream = malloc(sizeof(*stream));
    if (!stream) return stream;

    for (;;) {
        generate_temp_name(buffer);

        int fd = hydrogen_open(-1, buffer, L_tmpnam - 1, O_WRONLY | O_CREAT | O_EXCL | O_APPEND, 0666);
        if (fd < 0) {
            if (fd == -ERR_NOT_FOUND) continue;
            errno = -fd;
            free(stream);
            return NULL;
        }

        openfd(stream, fd);
        return stream;
    }
}

EXPORT char *tmpnam(char *s) {
    static char buffer[L_tmpnam];
    if (!s) s = buffer;

    for (;;) {
        generate_temp_name(s);

        hydrogen_stat_t buf;
        int error = hydrogen_stat(-1, s, L_tmpnam - 1, &buf, false);
        if (error == ERR_NOT_FOUND) return s;
        if (error) {
            errno = -error;
            return NULL;
        }
    }
}

static FILE *open_init_stream(int fd) {
    FILE *stream = malloc(sizeof(*stream));
    if (!stream) abort();
    openfd(stream, fd);
    return stream;
}

void init_stdio(void) {
    stdin = open_init_stream(0);
    stdout = open_init_stream(1);
    stderr = open_init_stream(2);
}

static int do_close(FILE *stream) {
    int error = hydrogen_close(stream->__fd);

    if (!error) {
        return 0;
    } else {
        errno = error;
        return EOF;
    }
}

EXPORT int fclose(FILE *stream) {
    int error = do_close(stream);
    __builtin_free(stream);
    return error;
}

EXPORT int fflush(UNUSED FILE *stream) {
    return 0;
}

static int mode_to_flags(const char *mode) {
    int flags = O_NODIR;

    if (mode[0]) {
        switch (mode[0]) {
        case 'r': flags |= O_RDONLY; break;
        case 'w': flags |= O_WRONLY | O_CREAT | O_TRUNC; break;
        case 'a': flags |= O_WRONLY | O_CREAT | O_APPEND; break;
        }

        if (mode[1]) {
            if (mode[1] == '+' || mode[2] == '+') flags |= O_WRONLY;
        }
    }

    return flags;
}

static FILE *open_into(FILE *stream, const char *filename, const char *mode) {
    int flags = mode_to_flags(mode);
    int fd = hydrogen_open(-1, filename, __builtin_strlen(filename), flags, 0666);
    if (fd < 0) {
        errno = -fd;
        free(stream);
        return NULL;
    }

    openfd(stream, fd);
    return stream;
}

EXPORT FILE *fopen(const char *restrict filename, const char *restrict mode) {
    FILE *stream = malloc(sizeof(*stream));
    if (!stream) return NULL;

    return open_into(stream, filename, mode);
}

EXPORT FILE *freopen(const char *restrict filename, const char *restrict mode, FILE *restrict stream) {
    do_close(stream);
    return open_into(stream, filename, mode);
}

EXPORT void setbuf(FILE *restrict stream, char *restrict buf) {
    UNUSED int ret = setvbuf(stream, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
    assert(ret == 0);
}

EXPORT int setvbuf(UNUSED FILE *restrict stream, UNUSED char *restrict buf, UNUSED int mode, UNUSED size_t size) {
    return 0; // TODO: Buffering
}

EXPORT int fgetc(FILE *stream) {
    unsigned char c;
    return fread(&c, sizeof(c), 1, stream) == 1 ? c : EOF;
}

EXPORT char *fgets(char *restrict s, int n, FILE *restrict stream) {
    if (n == 0) return NULL;

    char *start = s;

    for (int i = 1; i < n; i++) {
        int value = fgetc(stream);
        if (value == EOF) {
            if (s == start || (stream->__flags & STREAM_EOF) == 0) return NULL;
            break;
        }

        *s++ = (unsigned char)value;
        if (value == '\n') break;
    }

    *s = 0;
    return start;
}

EXPORT int fputc(int c, FILE *stream) {
    unsigned char v = c;
    return fwrite(&v, sizeof(v), 1, stream) == 1 ? v : EOF;
}

EXPORT int fputs(const char *restrict s, FILE *restrict stream) {
    size_t len = __builtin_strlen(s);
    return fwrite(s, 1, len, stream) == len ? 0 : EOF;
}

EXPORT int getc(FILE *stream) {
    return fgetc(stream);
}

EXPORT int getchar(void) {
    return getc(stdin);
}

EXPORT char *gets(char *s) {
    return fgets(s, INT_MAX, stdin);
}

EXPORT int putc(int c, FILE *stream) {
    return fputc(c, stream);
}

EXPORT int putchar(int c) {
    return putc(c, stdout);
}

EXPORT int puts(const char *s) {
    if (fputs(s, stdout) == EOF) return EOF;
    return putchar('\n');
}

EXPORT int ungetc(int c, FILE *stream) {
    if (c == EOF || stream->__push_count == sizeof(stream->__push_buffer)) return EOF;

    unsigned char v = c;

    stream->__push_buffer[sizeof(stream->__push_buffer) - ++stream->__push_count] = v;
    stream->__flags &= ~STREAM_EOF;

    return v;
}

static hydrogen_io_res_t do_read(FILE *stream, void *buffer, size_t count) {
    size_t extra = stream->__push_count;

    if (extra) {
        if (extra > count) extra = count;

        __builtin_memcpy(buffer, &stream->__push_buffer[sizeof(stream->__push_buffer) - stream->__push_count], extra);

        buffer += extra;
        count -= extra;

        if (count == 0) {
            stream->__push_count -= extra;
            return (hydrogen_io_res_t){extra, 0};
        }
    }

    hydrogen_io_res_t res = hydrogen_read(stream->__fd, buffer, count);
    if (res.error) return res;

    stream->__push_count -= extra;
    if (!res.transferred) stream->__flags |= STREAM_EOF;

    res.transferred += extra;
    return res;
}

EXPORT size_t fread(void *restrict ptr, size_t size, size_t nmemb, FILE *restrict stream) {
    size_t count = size * nmemb;
    if (!count) return 0;
    if (stream->__flags & STREAM_EOF) return 0;

    hydrogen_io_res_t res = do_read(stream, ptr, count);
    if (res.error) {
        errno = res.error;
        stream->__flags |= STREAM_ERR;
        return 0;
    }

    return res.transferred / size;
}

EXPORT size_t fwrite(const void *restrict ptr, size_t size, size_t nmemb, FILE *restrict stream) {
    size_t count = size * nmemb;
    if (!count) return 0;

    hydrogen_io_res_t res = hydrogen_write(stream->__fd, ptr, count);
    if (res.error) {
        errno = res.error;
        stream->__flags |= STREAM_ERR;
        return 0;
    }

    return res.transferred / size;
}

EXPORT int fgetpos(FILE *restrict stream, fpos_t *restrict pos) {
    int error = hydrogen_seek(stream->__fd, &pos->__offset, HYDROGEN_WHENCE_CUR);

    if (error == 0) {
        pos->__offset -= stream->__push_count;
        return 0;
    } else {
        errno = error;
        return 1;
    }
}

EXPORT int fseek(FILE *stream, long offset, int whence) {
    uint64_t off = offset;
    hydrogen_whence_t hwhence;

    switch (whence) {
    case SEEK_SET: hwhence = HYDROGEN_WHENCE_SET; break;
    case SEEK_CUR: hwhence = HYDROGEN_WHENCE_CUR; break;
    case SEEK_END: hwhence = HYDROGEN_WHENCE_END; break;
    default: __builtin_unreachable();
    }

    int error = hydrogen_seek(stream->__fd, &off, hwhence);
    if (error) {
        errno = error;
        return 1;
    }

    stream->__push_count = 0;
    stream->__flags &= ~STREAM_EOF;

    return 0;
}

EXPORT int fsetpos(FILE *stream, const fpos_t *pos) {
    uint64_t off = pos->__offset;
    int error = hydrogen_seek(stream->__fd, &off, HYDROGEN_WHENCE_SET);
    if (error) {
        errno = error;
        return 1;
    }

    stream->__push_count = 0;
    stream->__flags &= ~STREAM_EOF;

    return 0;
}

EXPORT long ftell(FILE *stream) {
    fpos_t pos;
    if (fgetpos(stream, &pos)) return -1L;
    return pos.__offset;
}

EXPORT void rewind(FILE *stream) {
    uint64_t off = 0;
    hydrogen_seek(stream->__fd, &off, HYDROGEN_WHENCE_SET); // discard error

    stream->__push_count = 0;
    stream->__flags &= ~(STREAM_EOF | STREAM_ERR);
}

EXPORT void clearerr(FILE *stream) {
    stream->__flags &= ~STREAM_ERR;
}

EXPORT int feof(FILE *stream) {
    return stream->__flags & STREAM_EOF;
}

EXPORT int ferror(FILE *stream) {
    return stream->__flags & STREAM_ERR;
}

EXPORT void perror(const char *s) {
    fprintf(stderr, "%s: %s\n", s, strerror(errno));
}
