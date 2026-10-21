#ifndef NANO_TCC_COMPAT_H
#define NANO_TCC_COMPAT_H

#include <stdint.h>
#include <stdarg.h>

// 1. Tipuri primitive POSIX
typedef unsigned long size_t;
typedef long ssize_t;
typedef long ptrdiff_t;
typedef long off_t;
typedef int mode_t;

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define RTLD_DEFAULT ((void*)0)

#ifndef NULL
#define NULL ((void*)0)
#endif

#define EINTR  4
#define ENOENT 2
#define EIO    5
#define ENOMEM 12
#define EACCES 13
#define EINVAL 22

static int nano_errno = 0;
#define errno nano_errno

#define O_RDONLY 00
#define O_WRONLY 01
#define O_RDWR   02
#define O_CREAT  0100
#define O_TRUNC  01000
#define O_BINARY 0

#define assert(x) ((void)0)
#define TCC_OUTPUT_BINARY TCC_OUTPUT_EXE

#ifndef CONFIG_TCC_SYSINCLUDEPATHS
#define CONFIG_TCC_SYSINCLUDEPATHS "/hdb/include:/include"
#endif

#ifndef CONFIG_TCC_CRT_PREFIX
#define CONFIG_TCC_CRT_PREFIX "/hdb/lib"
#endif

// Variable de mediu
static char *nano_environ[] = { (void*)0 };
#define environ nano_environ

// 2. Prevenim conflictul is_space între nano_string.h și tcc.h
#define is_space nano_libc_is_space

// 3. Includem librăria Nano OS
#include "nano_libc.h"

// Re-mapăm is_space pentru TCC
#undef is_space
#define is_space tcc_is_space

// 4. Implementăm funcțiile de memorie ca FUNCȚII REAL (nu macro-uri)
// Astfel, dacă TCC execută #undef free / #undef realloc, va găsi totuși declarația C
static inline void free(void* ptr) {
    if (ptr) nano_free(ptr);
}

static inline void* malloc(size_t size) {
    return nano_malloc(size);
}

static inline void* realloc(void* ptr, size_t size) {
    if (!ptr) return nano_malloc(size);
    void* new_ptr = nano_malloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, size);
        nano_free(ptr);
    }
    return new_ptr;
}

// 5. Structura FILE și I/O wrapper-e
typedef struct {
    char filename[64];
    uint32_t offset;
    uint32_t size;
    uint32_t is_open;
} FILE;

#define stdout ((FILE*)1)
#define stderr ((FILE*)2)
#define stdin  ((FILE*)0)

static inline FILE* fopen(const char* path, const char* mode) {
    FILE* f = (FILE*)nano_malloc(sizeof(FILE));
    if (!f) return NULL;
    memset(f, 0, sizeof(FILE));
    strcpy(f->filename, path);
    f->offset = 0;
    f->is_open = 1;
    if (mode && mode[0] == 'w') {
        nano_create_file(path, 0);
        f->size = 0;
    }
    return f;
}

static inline FILE* freopen(const char* path, const char* mode, FILE* stream) {
    (void)path; (void)mode;
    return stream;
}

static inline size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    if (!stream || !stream->is_open) return 0;
    uint32_t bytes_to_read = size * nmemb;
    int read_bytes = nano_read_file(stream->filename, (uint8_t*)ptr, bytes_to_read);
    if (read_bytes < 0) return 0;
    stream->offset += read_bytes;
    return read_bytes / size;
}

static inline size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    if (stream == stdout || stream == stderr) {
        char temp[512];
        uint32_t total = size * nmemb;
        if (total > 511) total = 511;
        memcpy(temp, ptr, total);
        temp[total] = '\0';
        nano_print(temp);
        return nmemb;
    }
    if (!stream || !stream->is_open) return 0;
    uint32_t bytes = size * nmemb;
    nano_write_file(stream->filename, (const uint8_t*)ptr, bytes);
    stream->offset += bytes;
    return nmemb;
}

static inline int fclose(FILE* stream) {
    if (stream && stream != stdout && stream != stderr) {
        stream->is_open = 0;
        nano_free(stream);
    }
    return 0;
}

static inline int fgetc(FILE* stream) {
    unsigned char c;
    if (fread(&c, 1, 1, stream) == 1)
        return c;
    return -1;
}

static inline int fseek(FILE* stream, long offset, int whence) {
    if (!stream) return -1;
    if (whence == SEEK_SET) {
        stream->offset = (uint32_t)offset;
    } else if (whence == SEEK_CUR) {
        stream->offset += (uint32_t)offset;
    } else if (whence == SEEK_END) {
        stream->offset = stream->size + (uint32_t)offset;
    }
    return 0;
}

static inline long ftell(FILE* stream) {
    if (!stream) return -1;
    return (long)stream->offset;
}

static inline FILE* fdopen(int fd, const char* mode) {
    (void)fd; (void)mode;
    return stdout;
}

static inline int open(const char* pathname, int flags, ...) {
    (void)pathname; (void)flags;
    return 3;
}

static inline int close(int fd) {
    (void)fd;
    return 0;
}

static inline ssize_t read(int fd, void* buf, size_t count) {
    (void)fd; (void)buf; (void)count;
    return 0;
}

static inline ssize_t write(int fd, const void* buf, size_t count) {
    (void)fd; (void)buf;
    return count;
}

static inline off_t lseek(int fd, off_t offset, int whence) {
    (void)fd; (void)whence;
    return offset;
}

static inline int unlink(const char* pathname) {
    return nano_delete_file(pathname);
}

static inline int remove(const char* pathname) {
    return nano_delete_file(pathname);
}

static inline char* getcwd(char* buf, size_t size) {
    if (buf && size > 0) {
        buf[0] = '/';
        buf[1] = '\0';
    }
    return buf;
}

static inline int execvp(const char* file, char* const argv[]) {
    (void)argv;
    return nano_exec(file);
}

// 6. Manipulări de șiruri
static inline char* realpath(const char* path, char* resolved_path) {
    if (!path) return NULL;
    if (!resolved_path) {
        resolved_path = (char*)nano_malloc(256);
    }
    if (resolved_path) {
        strcpy(resolved_path, path);
    }
    return resolved_path;
}

static inline char* strcat(char* dest, const char* src) {
    char* ptr = dest + strlen(dest);
    while ((*ptr++ = *src++));
    return dest;
}

static inline char* strpbrk(const char* s, const char* accept) {
    while (*s) {
        const char* a = accept;
        while (*a) {
            if (*s == *a) return (char*)s;
            a++;
        }
        s++;
    }
    return NULL;
}

static inline char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    return (c == 0) ? (char*)s : NULL;
}

static inline char* strrchr(const char* s, int c) {
    const char* last = NULL;
    do {
        if (*s == (char)c) last = s;
    } while (*s++);
    return (char*)last;
}

static inline char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        if (*haystack == *needle) {
            const char *h = haystack, *n = needle;
            while (*h && *n && *h == *n) { h++; n++; }
            if (!*n) return (char*)haystack;
        }
    }
    return NULL;
}

static inline int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t *p1 = (const uint8_t*)s1, *p2 = (const uint8_t*)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) return p1[i] - p2[i];
    }
    return 0;
}

static inline char* strerror(int errnum) {
    (void)errnum;
    return "Unknown error";
}

static inline long strtol(const char* nptr, char** endptr, int base) {
    (void)base;
    if (endptr) *endptr = (char*)nptr;
    return simple_atoi(nptr);
}

static inline unsigned long strtoul(const char* nptr, char** endptr, int base) {
    (void)base;
    if (endptr) *endptr = (char*)nptr;
    return (unsigned long)simple_atoi(nptr);
}

static inline unsigned long long strtoull(const char* nptr, char** endptr, int base) {
    (void)base;
    if (endptr) *endptr = (char*)nptr;
    return (unsigned long long)parse_int64(nptr);
}

static inline double strtod(const char* nptr, char** endptr) {
    if (endptr) *endptr = (char*)nptr;
    return 0.0;
}

static inline long double ldexpl(long double x, int exp) {
    (void)exp;
    return x;
}

// 7. Formatare Output & Sortare
#define printf(...) nano_printf(__VA_ARGS__)

static inline int sprintf(char* str, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int res = nano_vsnprintf(str, 4096, format, args);
    va_end(args);
    return res;
}

static inline int vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    return nano_vsnprintf(str, size, format, ap);
}

static inline int fprintf(FILE* stream, const char* format, ...) {
    va_list args;
    va_start(args, format);
    if (stream == stdout || stream == stderr) {
        char buf[512];
        nano_vsnprintf(buf, sizeof(buf), format, args);
        nano_print(buf);
    }
    va_end(args);
    return 0;
}

static inline int vfprintf(FILE* stream, const char* format, va_list ap) {
    if (stream == stdout || stream == stderr) {
        char buf[512];
        nano_vsnprintf(buf, sizeof(buf), format, ap);
        nano_print(buf);
    }
    return 0;
}

static inline int fflush(FILE* stream) {
    (void)stream;
    return 0;
}

static inline int fputs(const char* str, FILE* stream) {
    (void)stream;
    nano_print(str);
    return 0;
}

static inline int fputc(int c, FILE* stream) {
    (void)stream;
    char b[2] = {(char)c, 0};
    nano_print(b);
    return c;
}

static inline void qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) {
    char* p = (char*)base;
    char temp[256];
    if (size > sizeof(temp)) return;
    for (size_t i = 1; i < nmemb; i++) {
        for (size_t j = i; j > 0; j--) {
            char* a = p + (j - 1) * size;
            char* b = p + j * size;
            if (compar(a, b) > 0) {
                memcpy(temp, a, size);
                memcpy(a, b, size);
                memcpy(b, temp, size);
            } else {
                break;
            }
        }
    }
}

static inline char* getenv(const char* name) { (void)name; return NULL; }
static inline void exit(int status) { nano_sys_exit(status); }

static inline float strtof(const char* nptr, char** endptr) {
    if (endptr) *endptr = (char*)nptr;
    return 0.0f;
}

static inline long double strtold(const char* nptr, char** endptr) {
    if (endptr) *endptr = (char*)nptr;
    return 0.0L;
}

#endif