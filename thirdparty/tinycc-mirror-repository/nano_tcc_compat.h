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
#define O_TRUNC 01000
#define O_BINARY 0

#define assert(x) ((void)0)

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
typedef union {
    size_t size;
    long double align_long_double;
    void *align_pointer;
} NanoTCCAllocationHeader;

static inline void free(void* ptr) {
    if (ptr)
        nano_free((NanoTCCAllocationHeader *)ptr - 1);
}

static inline void* malloc(size_t size) {
    NanoTCCAllocationHeader *header;

    if (size > (size_t)-1 - sizeof(*header))
        return NULL;
    header = nano_malloc(sizeof(*header) + size);
    if (!header)
        return NULL;
    header->size = size;
    return header + 1;
}

static inline void* realloc(void* ptr, size_t size) {
    NanoTCCAllocationHeader *old_header, *new_header;
    size_t copy_size;

    if (!ptr)
        return malloc(size);
    if (!size) {
        free(ptr);
        return NULL;
    }
    if (size > (size_t)-1 - sizeof(*new_header))
        return NULL;

    old_header = (NanoTCCAllocationHeader *)ptr - 1;
    new_header = nano_malloc(sizeof(*new_header) + size);
    if (!new_header)
        return NULL;

    new_header->size = size;
    copy_size = old_header->size < size ? old_header->size : size;
    memcpy(new_header + 1, ptr, copy_size);
    nano_free(old_header);
    return new_header + 1;
}

// 5. Structura FILE și I/O wrapper-e
typedef struct {
    char filename[64];
    const char *output_path;
    uint8_t *output_data;
    uint32_t offset;
    uint32_t size;
    uint32_t output_capacity;
    uint32_t is_open;
    uint32_t is_output;
} FILE;

#define stdout ((FILE*)1)
#define stderr ((FILE*)2)
#define stdin  ((FILE*)0)

static const char *nano_tcc_pending_output_path;
static int nano_tcc_nas1_output;
static int nano_tcc_io_debug;

#define NANO_TCC_MAX_INPUTS 16
#define NANO_TCC_MAX_INPUT_SIZE (4u * 1024u * 1024u)

typedef struct {
    uint8_t *data;
    uint32_t size;
    uint32_t offset;
} NanoTCCInput;

static NanoTCCInput nano_tcc_inputs[NANO_TCC_MAX_INPUTS];

static inline void nano_tcc_set_nas1_output(int enabled) {
    nano_tcc_nas1_output = enabled;
}

static inline void nano_tcc_set_io_debug(int enabled) {
    nano_tcc_io_debug = enabled;
}

static inline int nano_tcc_output_append(FILE* stream, const void* data, uint32_t size) {
    uint32_t required, capacity;
    uint8_t *new_data;

    if (size > (uint32_t)-1 - stream->size)
        return -1;
    required = stream->size + size;
    if (required > stream->output_capacity) {
        capacity = stream->output_capacity ? stream->output_capacity : 256;
        while (capacity < required) {
            if (capacity > (uint32_t)-1 / 2) {
                capacity = required;
                break;
            }
            capacity *= 2;
        }
        new_data = (uint8_t*)nano_malloc(capacity);
        if (!new_data)
            return -1;
        if (stream->size)
            memcpy(new_data, stream->output_data, stream->size);
        nano_free(stream->output_data);
        stream->output_data = new_data;
        stream->output_capacity = capacity;
    }
    if (size)
        memcpy(stream->output_data + stream->size, data, size);
    stream->size = required;
    return 0;
}

static inline int nano_tcc_set_nas1_entry_offset(FILE* stream, uint32_t offset) {
    if (!stream || !stream->is_output || stream->size < 8)
        return -1;
    stream->output_data[4] = (uint8_t)offset;
    stream->output_data[5] = (uint8_t)(offset >> 8);
    stream->output_data[6] = (uint8_t)(offset >> 16);
    stream->output_data[7] = (uint8_t)(offset >> 24);
    return 0;
}

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
    if (size && nmemb > (uint32_t)-1 / size) return 0;
    uint32_t bytes = size * nmemb;
    if (stream->is_output)
        return nano_tcc_output_append(stream, ptr, bytes) == 0 ? nmemb : 0;
    nano_write_file(stream->filename, (const uint8_t*)ptr, bytes);
    stream->offset += bytes;
    return nmemb;
}

static inline int fclose(FILE* stream) {
    if (stream && stream != stdout && stream != stderr) {
        int result = 0;
        if (stream->is_output) {
            if (!nano_create_file(stream->output_path, stream->size) ||
                !nano_write_file(stream->output_path, stream->output_data, stream->size))
                result = -1;
            nano_free(stream->output_data);
        }
        stream->is_open = 0;
        nano_free(stream);
        return result;
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
    const char *output_path;
    int add_nas1_header;
    (void)mode;
    if (fd == 1)
        return stdout;
    if (!nano_tcc_pending_output_path)
        return NULL;
    output_path = nano_tcc_pending_output_path;
    nano_tcc_pending_output_path = NULL;
    add_nas1_header = nano_tcc_nas1_output;
    nano_tcc_nas1_output = 0;
    FILE* f = (FILE*)nano_malloc(sizeof(FILE));
    if (!f)
        return NULL;
    memset(f, 0, sizeof(FILE));
    f->output_path = output_path;
    f->is_open = 1;
    f->is_output = 1;
    if (add_nas1_header) {
        static const uint8_t nas1_header[8] = {'N', 'A', 'S', '1', 8, 0, 0, 0};
        if (nano_tcc_output_append(f, nas1_header, sizeof(nas1_header)) < 0) {
            nano_free(f);
            return NULL;
        }
    }
    return f;
}

static inline int open(const char* pathname, int flags, ...) {
    uint32_t capacity = 4096;
    uint8_t *data;
    int bytes_read, i;

    if ((flags & O_WRONLY) && pathname) {
        nano_tcc_pending_output_path = pathname;
        return 3;
    }
    if (!pathname)
        return -1;

    for (i = 0; i < NANO_TCC_MAX_INPUTS; ++i)
        if (!nano_tcc_inputs[i].data)
            break;
    if (i == NANO_TCC_MAX_INPUTS)
        return -1;

    data = nano_malloc(capacity);
    if (!data)
        return -1;

    for (;;) {
        bytes_read = nano_read_file(pathname, data, capacity);
        if (bytes_read < 0) {
            nano_free(data);
            return -1;
        }
        if ((uint32_t)bytes_read < capacity)
            break;
        if (capacity == NANO_TCC_MAX_INPUT_SIZE) {
            nano_free(data);
            return -1;
        }

        {
            uint32_t new_capacity = capacity * 2;
            uint8_t *new_data;
            if (new_capacity > NANO_TCC_MAX_INPUT_SIZE)
                new_capacity = NANO_TCC_MAX_INPUT_SIZE;
            new_data = nano_malloc(new_capacity);
            if (!new_data) {
                nano_free(data);
                return -1;
            }
            nano_free(data);
            data = new_data;
            capacity = new_capacity;
        }
    }

    nano_tcc_inputs[i].data = data;
    nano_tcc_inputs[i].size = (uint32_t)bytes_read;
    nano_tcc_inputs[i].offset = 0;
    if (nano_tcc_io_debug)
        nano_printf("TCC: opened %s, read %d bytes\n", pathname, bytes_read);
    return i + 4;
}

static inline int close(int fd) {
    int i = fd - 4;
    if (i >= 0 && i < NANO_TCC_MAX_INPUTS && nano_tcc_inputs[i].data) {
        nano_free(nano_tcc_inputs[i].data);
        memset(&nano_tcc_inputs[i], 0, sizeof(nano_tcc_inputs[i]));
    }
    return 0;
}

static inline ssize_t read(int fd, void* buf, size_t count) {
    int i = fd - 4;
    uint32_t available, bytes_to_read;

    if (i < 0 || i >= NANO_TCC_MAX_INPUTS || !nano_tcc_inputs[i].data)
        return -1;
    if (!buf || !count)
        return 0;

    available = nano_tcc_inputs[i].size - nano_tcc_inputs[i].offset;
    bytes_to_read = count < available ? (uint32_t)count : available;
    if (bytes_to_read) {
        memcpy(buf, nano_tcc_inputs[i].data + nano_tcc_inputs[i].offset,
               bytes_to_read);
        nano_tcc_inputs[i].offset += bytes_to_read;
    }
    return bytes_to_read;
}

static inline ssize_t write(int fd, const void* buf, size_t count) {
    (void)fd; (void)buf;
    return count;
}

static inline off_t lseek(int fd, off_t offset, int whence) {
    int i = fd - 4;
    long base, position;

    if (i < 0 || i >= NANO_TCC_MAX_INPUTS || !nano_tcc_inputs[i].data)
        return -1;

    switch (whence) {
    case SEEK_SET:
        base = 0;
        break;
    case SEEK_CUR:
        base = nano_tcc_inputs[i].offset;
        break;
    case SEEK_END:
        base = nano_tcc_inputs[i].size;
        break;
    default:
        return -1;
    }

    position = base + offset;
    if (position < 0 || (unsigned long)position > nano_tcc_inputs[i].size)
        return -1;
    nano_tcc_inputs[i].offset = (uint32_t)position;
    return position;
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

static inline unsigned long long nano_strtoull(const char* nptr, char** endptr, int base) {
    const char *p = nptr;
    unsigned long long value = 0;
    int negative = 0;
    int digits = 0;

    while (*p == ' ' || (*p >= '\t' && *p <= '\r'))
        p++;
    if (*p == '+' || *p == '-') {
        negative = *p == '-';
        p++;
    }

    if (base == 0) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            base = 16;
            p += 2;
        } else {
            base = p[0] == '0' ? 8 : 10;
        }
    } else if (base == 16 && p[0] == '0' &&
               (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
    }

    if (base < 2 || base > 36) {
        if (endptr)
            *endptr = (char*)nptr;
        return 0;
    }

    for (;;) {
        int digit;
        if (*p >= '0' && *p <= '9')
            digit = *p - '0';
        else if (*p >= 'a' && *p <= 'z')
            digit = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'Z')
            digit = *p - 'A' + 10;
        else
            break;
        if (digit >= base)
            break;
        value = value * (unsigned)base + (unsigned)digit;
        digits = 1;
        p++;
    }

    if (endptr)
        *endptr = (char*)(digits ? p : nptr);
    return negative ? 0 - value : value;
}

static inline long strtol(const char* nptr, char** endptr, int base) {
    return (long)nano_strtoull(nptr, endptr, base);
}

static inline unsigned long strtoul(const char* nptr, char** endptr, int base) {
    return (unsigned long)nano_strtoull(nptr, endptr, base);
}

static inline unsigned long long strtoull(const char* nptr, char** endptr, int base) {
    return nano_strtoull(nptr, endptr, base);
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
    if (stream && stream->is_output) {
        unsigned char byte = (unsigned char)c;
        return nano_tcc_output_append(stream, &byte, 1) == 0 ? byte : -1;
    }
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