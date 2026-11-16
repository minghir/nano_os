#include "nano_libc.h"
#include "stdio.h"
#include "stdlib.h"
#include "time.h"
#include "math.h"
#include "string.h"
#include "ctype.h"
#include <stdarg.h>

// ====================================================================
// TABELA INTERNĂ DE FIȘIERE ȘI DESCRIPTORII POSIX (fd)
// ====================================================================

#define MAX_OPEN_FILES 16
#define NANO_O_APPEND 0x40000000

typedef struct {
    int active;
    int flags;
    char path[128];
    uint32_t offset;
    uint32_t size;
} OpenFileEntry;

static OpenFileEntry file_table[MAX_OPEN_FILES];

// Inițializăm fluxurile standard la pornire (0=stdin, 1=stdout, 2=stderr)
static FILE _stdin  = {0, 0, 0};
static FILE _stdout = {1, 0, 0};
static FILE _stderr = {2, 0, 0};

FILE *stdin  = &_stdin;
FILE *stdout = &_stdout;
FILE *stderr = &_stderr;

// ====================================================================
// Nivel Jos: POSIX File Descriptors (open, close, read, write, lseek)
// ====================================================================

int open(const char *pathname, int flags, ...) {
    if (!pathname || strlen(pathname) >= sizeof(file_table[0].path)) return -1;

    int64_t file_size = nano_get_file_size(pathname);
    if (file_size < 0) {
        if (!(flags & 1) || !nano_create_file(pathname, 0, 0666)) return -1;
    }

    int fd = -1;
    // Căutăm un slot liber începând de la 3 (0, 1, 2 sunt rezervate pentru stdin, stdout, stderr)
    for (int i = 3; i < MAX_OPEN_FILES; i++) {
        if (!file_table[i].active) {
            fd = i;
            break;
        }
    }
    if (fd == -1) return -1; // Prea multe fișiere deschise

    file_table[fd].active = 1;
    file_table[fd].flags = flags;
    strcpy(file_table[fd].path, pathname);
    file_table[fd].offset = 0;
    
    return fd;
}

int close(int fd) {
    if (fd < 3 || fd >= MAX_OPEN_FILES || !file_table[fd].active) return -1;
    file_table[fd].active = 0;
    return 0;
}

ssize_t read(int fd, void *buf, size_t count) {
    if (fd == 0) return 0; // stdin
    if (fd < 3 || fd >= MAX_OPEN_FILES || !file_table[fd].active) return -1;
    if (count == 0) return 0;
    if (!buf) return -1;
    if (count > 0xFFFFFFFFu) count = 0xFFFFFFFFu;

    OpenFileEntry *entry = &file_table[fd];
    if ((uint64_t)entry->offset + count > 0xFFFFFFFFu) return -1;
    int bytes = nano_read_file_at(entry->path, (uint8_t*)buf, entry->offset, (uint32_t)count);
    if (bytes < 0) return -1;
    entry->offset += (uint32_t)bytes;
    return bytes;
}

ssize_t write(int fd, const void *buf, size_t count) {
    if (count == 0) return 0;
    if (!buf || count > 0xFFFFFFFFu) return -1;
    if (fd == 1 || fd == 2) {
        nano_print((const char*)buf);
        return count;
    }
    if (fd < 3 || fd >= MAX_OPEN_FILES || !file_table[fd].active) return -1;

    OpenFileEntry *entry = &file_table[fd];
    if (entry->flags & NANO_O_APPEND) {
        int64_t file_size = nano_get_file_size(entry->path);
        if (file_size < 0 || file_size > 0xFFFFFFFFu) return -1;
        entry->offset = (uint32_t)file_size;
    }
    if ((uint64_t)entry->offset + count > 0xFFFFFFFFu) return -1;
    int bytes = nano_write_file_at(entry->path, (const uint8_t*)buf, entry->offset, (uint32_t)count);
    if (bytes < 0) return -1;
    entry->offset += (uint32_t)bytes;
    return bytes;
}

long lseek(int fd, long offset, int whence) {
    if (fd < 3 || fd >= MAX_OPEN_FILES || !file_table[fd].active) return -1;
    OpenFileEntry *entry = &file_table[fd];
    int64_t new_offset;

    if (whence == SEEK_SET) {
        new_offset = offset;
    } else if (whence == SEEK_CUR) {
        new_offset = (int64_t)entry->offset + offset;
    } else if (whence == SEEK_END) {
        int64_t file_size = nano_get_file_size(entry->path);
        if (file_size < 0) return -1;
        new_offset = file_size + offset;
    } else {
        return -1;
    }

    if (new_offset < 0 || new_offset > 0xFFFFFFFFu) return -1;
    entry->offset = (uint32_t)new_offset;
    return (long)new_offset;
}

// ====================================================================
// Nivel Sus: Standard C FILE * (Construit peste fd)
// ====================================================================

FILE *fopen(const char *filename, const char *mode) {
    if (!filename || !mode || (mode[0] != 'r' && mode[0] != 'w' && mode[0] != 'a')) {
        return NULL;
    }

    int flags = mode[0] == 'w' ? 1 : mode[0] == 'a' ? 1 | NANO_O_APPEND : 0;

    int fd = open(filename, flags);
    if (fd < 0) return NULL;

    FILE *stream = (FILE*)nano_malloc(sizeof(FILE));
    if (!stream) {
        close(fd);
        return NULL;
    }

    stream->fd = fd;
    stream->flags = flags;
    stream->offset = 0;
    if (mode[0] == 'w' && !nano_truncate_file(filename, 0)) {
        close(fd);
        nano_free(stream);
        return NULL;
    }
    if (mode[0] == 'a') {
        int64_t file_size = nano_get_file_size(filename);
        if (file_size < 0 || file_size > 0xFFFFFFFFu) {
            close(fd);
            nano_free(stream);
            return NULL;
        }
        file_table[fd].offset = (uint32_t)file_size;
        stream->offset = (long)file_size;
    }
    return stream;
}

int fclose(FILE *stream) {
    if (!stream) return -1;
    if (stream == stdin || stream == stdout || stream == stderr) return -1;
    if (stream->fd >= 3 && close(stream->fd) < 0) return -1;
    nano_free(stream);
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (size == 0 || nmemb == 0) return 0;
    if (!stream || !ptr || stream->fd < 0) return 0;
    if (nmemb > (size_t)-1 / size) {
        stream->flags |= 0x20;
        return 0;
    }

    size_t total = size * nmemb;
    if (stream == stdin) {
        size_t bytes = 0;
        while (bytes < total) {
            int c = fgetc(stream);
            if (c == EOF) break;
            ((unsigned char*)ptr)[bytes++] = (unsigned char)c;
        }
        return bytes / size;
    }

    ssize_t bytes = read(stream->fd, ptr, total);
    if (bytes < 0) {
        stream->flags |= 0x20;
        return 0;
    }
    if ((size_t)bytes < total) stream->flags |= 0x10;
    if (bytes > 0) stream->offset += bytes;
    return (size_t)bytes / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (size == 0 || nmemb == 0) return 0;
    if (!stream || !ptr || stream->fd < 0) return 0;
    if (nmemb > (size_t)-1 / size) {
        stream->flags |= 0x20;
        return 0;
    }

    size_t total = size * nmemb;
    ssize_t bytes = write(stream->fd, ptr, total);
    if (bytes < 0) {
        stream->flags |= 0x20;
        return 0;
    }
    if ((size_t)bytes < total) stream->flags |= 0x20;
    stream->offset += bytes;
    return (size_t)bytes / size;
}

long ftell(FILE *stream) {
    if (!stream) return -1;
    return stream->offset;
}

int fseek(FILE *stream, long offset, int whence) {
    if (!stream) return -1;
    long res = lseek(stream->fd, offset, whence);
    if (res < 0) return -1;
    stream->offset = res;
    stream->flags &= ~0x10;
    return 0;
}

int remove(const char *filename) {
    return nano_delete_file(filename);
}

int rename(const char *oldname, const char *newname) {
    (void)oldname;
    (void)newname;
    return -1;
}

// ====================================================================
// FUNCTII AJUTĂTOARE PENTRU FORMATĂRI INTERNE
// ====================================================================

static char* print_num(char* buffer, uint64_t unum, int base, int is_negative) {
    char temp[64];
    int i = 0;

    do {
        int rem = unum % base;
        temp[i++] = (rem < 10) ? (rem + '0') : (rem - 10 + 'a');
        unum /= base;
    } while (unum > 0);

    if (is_negative) {
        temp[i++] = '-';
    }

    int j = 0;
    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = '\0';
    
    return buffer;
}

// ====================================================================
// NUCLEUL DE FORMATARE: vsnprintf
// ====================================================================

int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    size_t written = 0;
    if (size == 0) return 0;

    while (*format && written < size - 1) {
        if (*format == '%') {
            format++;
            int length_modifier = 0;
            if (*format == 'l') {
                length_modifier = 1;
                format++;
                if (*format == 'l') {
                    length_modifier = 2;
                    format++;
                }
            }
            switch (*format) {
                case 'c': {
                    char c = (char)va_arg(ap, int);
                    str[written++] = c;
                    break;
                }
                case 's': {
                    char *s = va_arg(ap, char*);
                    if (!s) s = "(null)";
                    while (*s && written < size - 1) {
                        str[written++] = *s++;
                    }
                    break;
                }
                case 'd':
                case 'i': {
                    int64_t num;
                    if (length_modifier == 2) {
                        num = va_arg(ap, long long);
                    } else if (length_modifier == 1) {
                        num = va_arg(ap, long);
                    } else {
                        num = va_arg(ap, int);
                    }
                    int is_negative = num < 0;
                    uint64_t magnitude = is_negative
                                       ? (uint64_t)(-(num + 1)) + 1
                                       : (uint64_t)num;
                    char buf[32];
                    print_num(buf, magnitude, 10, is_negative);
                    char *b = buf;
                    while (*b && written < size - 1) str[written++] = *b++;
                    break;
                }
                case 'u': {
                    uint64_t num;
                    if (length_modifier == 2) {
                        num = va_arg(ap, unsigned long long);
                    } else if (length_modifier == 1) {
                        num = va_arg(ap, unsigned long);
                    } else {
                        num = va_arg(ap, unsigned int);
                    }
                    char buf[32];
                    print_num(buf, num, 10, 0);
                    char *b = buf;
                    while (*b && written < size - 1) str[written++] = *b++;
                    break;
                }
                case 'x':
                case 'X': {
                    uint64_t num;
                    if (length_modifier == 2) {
                        num = va_arg(ap, unsigned long long);
                    } else if (length_modifier == 1) {
                        num = va_arg(ap, unsigned long);
                    } else {
                        num = va_arg(ap, unsigned int);
                    }
                    char buf[32];
                    print_num(buf, num, 16, 0);
                    if (*format == 'X') {
                        for (char *digit = buf; *digit; digit++) {
                            if (*digit >= 'a' && *digit <= 'f') *digit -= 'a' - 'A';
                        }
                    }
                    char *b = buf;
                    while (*b && written < size - 1) str[written++] = *b++;
                    break;
                }
                case '%': {
                    str[written++] = '%';
                    break;
                }
                default: {
                    str[written++] = '%';
                    if (written < size - 1) str[written++] = *format;
                    break;
                }
            }
        } else {
            str[written++] = *format;
        }
        format++;
    }
    
    str[written] = '\0';
    return written;
}

// ====================================================================
// WRAPPERS STANDARD PENTRU FORMATARE ȘI AFIȘARE
// ====================================================================

int snprintf(char *str, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

int sprintf(char *str, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, 1024 * 1024, format, ap); 
    va_end(ap);
    return ret;
}

int printf(const char *format, ...) {
    char buf[1024];
    va_list ap;
    
    va_start(ap, format);
    int len = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    nano_print(buf);
    
    return len;
}

int fprintf(FILE *stream, const char *format, ...) {
    if (!stream) return -1;
    char buf[1024];
    va_list ap;

    va_start(ap, format);
    int len = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    if (stream == stderr || stream == stdout) {
        nano_print(buf);
    } else {
        if (fwrite(buf, 1, (size_t)len, stream) != (size_t)len) return -1;
    }

    return len;
}

int vfprintf(FILE *stream, const char *format, va_list ap) {
    if (!stream) return -1;
    char buf[1024];
    int len = vsnprintf(buf, sizeof(buf), format, ap);
    if (len < 0) return -1;

    // Dacă stream-ul este stdout, stderr sau NULL, afișăm pe ecranul Nano OS
    if (stream == stdout || stream == stderr || stream->fd == 1 || stream->fd == 2) {
        nano_print(buf);
    } else {
        if (fwrite(buf, 1, (size_t)len, stream) != (size_t)len) return -1;
    }

    return len;
}

// ====================================================================
// FUNCȚII DE CONSOLĂ EXTRA (fflush, puts, putchar)
// ====================================================================

int fflush(FILE *stream) {
    (void)stream;
    return 0;
}

int puts(const char *s) {
    nano_print(s);
    nano_print("\n");
    return 0;
}

int putchar(int c) {
    char str[2];
    str[0] = (char)c;
    str[1] = '\0';
    nano_print(str);
    return c;
}

// ====================================================================
// IMPLEMENTARE SIMPLIFICATĂ SCANF (Pentru sscanf)
// ====================================================================

static long int libc_parse_long(const char *nptr, char **endptr, int base);

static int libc_vsscanf(const char *str, const char *format, va_list ap) {
    if (!str || !format) return EOF;
    const char *input = str;
    int assigned = 0;

    while (*format) {
        if (isspace((unsigned char)*format)) {
            while (isspace((unsigned char)*format)) format++;
            while (isspace((unsigned char)*input)) input++;
            continue;
        }
        if (*format != '%') {
            if (*input != *format) break;
            input++;
            format++;
            continue;
        }

        format++;
        if (*format == '%') {
            if (*input != '%') break;
            input++;
            format++;
            continue;
        }

        int suppress = 0;
        if (*format == '*') {
            suppress = 1;
            format++;
        }
        size_t width = 0;
        while (isdigit((unsigned char)*format)) {
            width = width * 10 + (size_t)(*format++ - '0');
        }
        if (width == 0) width = (size_t)-1;

        char conversion = *format++;
        if (conversion != 'c') {
            while (isspace((unsigned char)*input)) input++;
        }

        if (conversion == 'd' || conversion == 'i' || conversion == 'u' ||
            conversion == 'x' || conversion == 'X') {
            int base = conversion == 'i' ? 0 :
                       (conversion == 'x' || conversion == 'X') ? 16 : 10;
            char *end;
            long int value = libc_parse_long(input, &end, base);
            if (end == input) break;
            if (!suppress) {
                if (conversion == 'u') *va_arg(ap, unsigned int *) = (unsigned int)value;
                else *va_arg(ap, int *) = (int)value;
                assigned++;
            }
            input = end;
        } else if (conversion == 's') {
            if (!*input) break;
            char *dest = suppress ? NULL : va_arg(ap, char *);
            size_t copied = 0;
            while (*input && !isspace((unsigned char)*input) && copied < width) {
                if (dest) dest[copied] = *input;
                input++;
                copied++;
            }
            if (dest) dest[copied] = '\0';
            if (!suppress) assigned++;
        } else if (conversion == 'c') {
            size_t count = width == (size_t)-1 ? 1 : width;
            char *dest = suppress ? NULL : va_arg(ap, char *);
            size_t copied = 0;
            while (copied < count && *input) {
                if (dest) dest[copied] = *input;
                input++;
                copied++;
            }
            if (copied != count) break;
            if (!suppress) assigned++;
        } else if (conversion == 'n') {
            if (!suppress) *va_arg(ap, int *) = (int)(input - str);
        } else {
            break;
        }
    }

    return assigned;
}

int sscanf(const char *str, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int assigned = libc_vsscanf(str, format, ap);
    va_end(ap);
    return assigned;
}

// ====================================================================
// IMPLEMENTARE SCANF (Citire formatată de la tastatură / stdin)
// ====================================================================

// Presupunem că avem un buffer intern pentru linia citită de la tastatură
int scanf(const char *format, ...) {
    char input_buf[256];
    
    // Citim o linie completă de la tastatură folosind funcția ta nativă de readline
    // (Dacă folosești nano_readline, o apelăm direct)
    nano_readline(input_buf, sizeof(input_buf));

    // Folosim logica de parasare pe care o are deja sscanf pe acest buffer citit
    va_list ap;
    va_start(ap, format);
    
    // Putem refolosi codul de parasare din sscanf mutându-l într-o funcție internă vsscanf,
    // sau apelând direct logica de mai jos:
    int assigned = 0;
    const char *str = input_buf;

    while (*format && *str) {
        if (*format == ' ') {
            while (*str == ' ' || *str == '\t' || *str == '\n') str++;
            format++;
            continue;
        }

        if (*format == '%') {
            format++;
            if (*format == 'd' || *format == 'i') {
                int *val = va_arg(ap, int *);
                *val = (int)parse_int64(str);
                if (*str == '-' || *str == '+') str++;
                while (*str >= '0' && *str <= '9') str++;
                assigned++;
            } else if (*format == 'x' || *format == 'X') {
                int *val = va_arg(ap, int *);
                if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
                    str += 2;
                }
                int res = 0;
                while (1) {
                    char c = *str;
                    int digit = -1;
                    if (c >= '0' && c <= '9') digit = c - '0';
                    else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
                    else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
                    else break;
                    res = res * 16 + digit;
                    str++;
                }
                *val = res;
                assigned++;
            } else if (*format == 's') {
                char *buf = va_arg(ap, char *);
                while (*str && *str != ' ' && *str != '\t' && *str != '\n') {
                    *buf++ = *str++;
                }
                *buf = '\0';
                assigned++;
            }
            format++;
        } else {
            if (*str == *format) {
                str++;
                format++;
            } else {
                break;
            }
        }
    }

    va_end(ap);
    return assigned;
}

// ====================================================================
// IMPLEMENTARE CTYPE.H (Clasificare și conversie caractere)
// ====================================================================

int isalpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int isdigit(int c) {
    return (c >= '0' && c <= '9');
}

int isalnum(int c) {
    return isalpha(c) || isdigit(c);
}

int isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f');
}

int isupper(int c) {
    return (c >= 'A' && c <= 'Z');
}

int islower(int c) {
    return (c >= 'a' && c <= 'z');
}

int isprint(int c) {
    return (c >= 32 && c <= 126);
}

int ispunct(int c) {
    return isprint(c) && !isalnum(c) && !isspace(c);
}

int isxdigit(int c) {
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int toupper(int c) {
    if (islower(c)) {
        return c - 32;
    }
    return c;
}

int tolower(int c) {
    if (isupper(c)) {
        return c + 32;
    }
    return c;
}

// ====================================================================
// IMPLEMENTARE STDLIB.H (RNG, Conversii, Control)
// ====================================================================

// Generator de numere aleatoare bazat pe Linear Congruential Generator (LCG)
static unsigned long next_rand = 1;

int rand(void) {
    next_rand = next_rand * 1103515245 + 12345;
    return (unsigned int)(next_rand / 65536) % (RAND_MAX + 1);
}

void srand(unsigned int seed) {
    next_rand = seed;
}

// Conversii standard
int atoi(const char *nptr) {
    return (int)parse_int64(nptr);
}

long atol(const char *nptr) {
    return (long)parse_int64(nptr);
}

// Funcții matematice simple
int abs(int j) {
    return (j < 0) ? -j : j;
}

long labs(long j) {
    return (j < 0) ? -j : j;
}

// Variabile de mediu (stubs sigure pentru aplicații care cer getenv)
char *getenv(const char *name) {
    (void)name;
    // Returnăm valori default pentru mediu
    return NULL; 
}

// Funcția exit (folosește deja syscall-ul tău de ieșire)
void exit(int status) {
    nano_sys_exit(status);
}

int system(const char *command) {
    (void)command;
    return -1; // Indică faptul că rularea de comenzi externe nu este suportată direct
}

// ====================================================================
// IMPLEMENTARE ATOF (Conversie string la double)
// ====================================================================

double atof(const char *nptr) {
    return strtod(nptr, NULL);
}


void *malloc(size_t size) {
    void *ptr = nano_malloc(size);
    if (!ptr) {
        // Dacă vezi asta pe ecran, înseamnă că heap-ul a rămas fără memorie!
        // (Sau heap_init nu a fost apelat cum trebuie pentru procese)
		printf("Sunt in malloc!\n");
    }
    return ptr;
}

void free(void *ptr) {
    nano_free(ptr);
}

void *calloc(size_t nmemb, size_t size) {
    if (size != 0 && nmemb > (size_t)-1 / size) return NULL;
    size_t total = nmemb * size;
    void *ptr = nano_malloc(total);
    if (ptr) {
        // Poți folosi un memset simplu sau buclă
        unsigned char *p = (unsigned char *)ptr;
        for (size_t i = 0; i < total; i++) p[i] = 0;
    }
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) {
        return malloc(size);
    }
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    typedef struct __attribute__((aligned(16))) BlockHeader {
        size_t size;
        int is_free;
        uint32_t owner_pid;
        struct BlockHeader* next;
    } BlockHeader;

    BlockHeader* header = (BlockHeader*)ptr - 1;
    size_t old_size = header->size;

    // Dacă noua dimensiune este mai mică sau egală cu cea veche, putem returna direct același bloc
    if (size <= old_size) {
        return ptr;
    }

    // Alocăm un nou bloc de memorie cu dimensiunea cerută
    void *new_ptr = malloc(size);
    if (!new_ptr) {
        return NULL; // Out of memory
    }

    // Copiem doar vechea dimensiune (evitând citirea în afara limitelor!)
    unsigned char *src = (unsigned char *)ptr;
    unsigned char *dst = (unsigned char *)new_ptr;
    for (size_t i = 0; i < old_size; i++) {
        dst[i] = src[i];
    }

    // Eliberăm vechiul bloc
    free(ptr);

    return new_ptr;
}

int tolower_char(int c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2 || tolower_char(*s1) == tolower_char(*s2))) {
        s1++;
        s2++;
    }
    return tolower_char(*(unsigned char *)s1) - tolower_char(*(unsigned char *)s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
    if (n == 0) return 0;
    while (n-- > 0 && *s1 && (*s1 == *s2 || tolower_char(*s1) == tolower_char(*s2))) {
        if (n == 0) return 0;
        s1++;
        s2++;
    }
    return tolower_char(*(unsigned char *)s1) - tolower_char(*(unsigned char *)s2);
}

char *strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *new_s = (char *)malloc(len);
    if (new_s) {
        strcpy(new_s, s);
    }
    return new_s;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == 0) ? (char *)s : NULL;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    do {
        if (*s == (char)c) last = s;
    } while (*s++);
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        if (*haystack == *needle) {
            const char *h = haystack, *n = needle;
            while (*h && *n && *h == *n) { h++; n++; }
            if (!*n) return (char *)haystack;
        }
    }
    return NULL;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for ( ; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

int *__errno_location(void) {
    static int nano_libc_errno = 0;
    return &nano_libc_errno;
}

int mkdir(const char *pathname, int mode) {
    return nano_mkdir(pathname, (uint16_t)mode);
}

void *__memset_chk(void *s, int c, size_t n, size_t destlen) {
    (void)destlen;
    unsigned char *p = s;
    for (size_t i = 0; i < n; i++) p[i] = (unsigned char)c;
    return s;
}

void *__memcpy_chk(void *dest, const void *src, size_t n, size_t destlen) {
    (void)destlen;
    unsigned char *d = dest;
    const unsigned char *s = src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}
// ====================================================================
// MEMMOVE (Copiere sigură cu suprapunere de memorie)
// ====================================================================

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (d == s) {
        return dest;
    }

    if (d < s) {
        // Dacă destinația e înaintea sursei, copiem normal (înainte)
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else {
        // Dacă destinația e după sursă, copiem de la coadă la cap
        // pentru a nu suprascrie datele din sursă înainte de a le citi
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }

    return dest;
}

// ====================================================================
// IMPLEMENTARE TIME.H (Timp, Ceas și Pauze)
// ====================================================================

time_t time(time_t *tloc) {
    // Presupunem că nano_get_ticks() returnează milisecundele de la pornire
    time_t seconds = (time_t)(nano_get_ticks() / 1000);
    if (tloc) {
        *tloc = seconds;
    }
    return seconds;
}

clock_t clock(void) {
    // Returnează numărul de tick-uri consumate de proces
    return (clock_t)nano_get_ticks();
}

unsigned int sleep(unsigned int seconds) {
    nano_sleep((uint32_t)(seconds * 1000));
    return 0;
}

int usleep(unsigned long usec) {
    nano_sleep((uint32_t)(usec / 1000));
    return 0;
}

// Stubs simple pentru structura tm (în caz că un program le cere la linkare)
static struct tm current_tm;

struct tm *localtime(const time_t *timer) {
    (void)timer;
    // Valori default sigure sau legate de structura ta DateTime din kernel
    current_tm.tm_sec = 0;
    current_tm.tm_min = 0;
    current_tm.tm_hour = 12;
    current_tm.tm_mday = 1;
    current_tm.tm_mon = 0;
    current_tm.tm_year = 126; // Anul 2026 (2026 - 1900)
    return &current_tm;
}

struct tm *gmtime(const time_t *timer) {
    return localtime(timer);
}

char *asctime(const struct tm *timeptr) {
    (void)timeptr;
    return "Thu Apr  3 12:00:00 2026\n";
}

char *ctime(const time_t *timer) {
    return asctime(localtime(timer));
}

// ====================================================================
// COMPLETĂRI STRING.H
// ====================================================================

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    if (n == 0) return 0;
    while (n-- > 0 && *s1 && (*s1 == *s2)) {
        if (n == 0 || *s1 == '\0') break;
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

char *strncat(char *dest, const char *src, size_t n) {
    char *ptr = dest + strlen(dest);
    size_t i = 0;
    while (i < n && src[i] != '\0') {
        ptr[i] = src[i];
        i++;
    }
    ptr[i] = '\0';
    return dest;
}

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = s;
    for (size_t i = 0; i < n; i++) {
        if (p[i] == (unsigned char)c) {
            return (void *)&p[i];
        }
    }
    return NULL;
}

char *strerror(int errnum) {
    (void)errnum;
 
   return "Unknown error";
}

// ====================================================================
// IMPLEMENTARE MATH.H (Matematică în virgulă mobilă)
// ====================================================================

int isnan(double x) {
    return x != x;
}

int isinf(double x) {
    return !isnan(x) && (x + x == x);
}

int finite(double x) {
    return !isnan(x) && !isinf(x);
}

double fabs(double x) {
    return (x < 0.0) ? -x : x;
}

double ceil(double x) {
    long long int_part = (long long)x;
    if (x > 0.0 && x != (double)int_part) {
        return (double)(int_part + 1);
    }
    return (double)int_part;
}

double floor(double x) {
    long long int_part = (long long)x;
    if (x < 0.0 && x != (double)int_part) {
        return (double)(int_part - 1);
    }
    return (double)int_part;
}

// Rădăcina pătrată folosind metoda lui Newton-Raphson
double sqrt(double x) {
    if (x < 0.0) return NAN;
    if (x == 0.0 || x == 1.0) return x;
    
    double guess = x / 2.0;
    double prev = 0.0;
    
    while (guess != prev) {
        prev = guess;
        guess = (guess + x / guess) / 2.0;
    }
    return guess;
}

// Restul împărțirii (fmod)
double fmod(double x, double y) {
    if (y == 0.0) return NAN;
    long long quot = (long long)(x / y);
    return x - (double)quot * y;
}

// Normalizarea unghiului în intervalul [-PI, PI] pentru aproximări trigonometrice
static double normalize_angle(double x) {
    while (x > M_PI) x -= 2.0 * M_PI;
    while (x < -M_PI) x += 2.0 * M_PI;
    return x;
}

// Aproximare eficientă pentru sine (Seria Taylor / Bhaskara I)
double sin(double x) {
    x = normalize_angle(x);
    // Taylor: x - x^3/3! + x^5/5! - x^7/7!
    double x2 = x * x;
    double term = x;
    double sum = term;
    
    term *= -x2 / (2.0 * 3.0);
    sum += term;
    term *= -x2 / (4.0 * 5.0);
    sum += term;
    term *= -x2 / (6.0 * 7.0);
    sum += term;
    term *= -x2 / (8.0 * 9.0);
    sum += term;
    
    return sum;
}

// Cosinus bazat pe identitatea cos(x) = sin(x + PI/2)
double cos(double x) {
    return sin(x + M_PI_2);
}

// Tangentă
double tan(double x) {
    double c = cos(x);
    if (c == 0.0) return INFINITY;
    return sin(x) / c;
}

// Aproximare pentru atan2
double atan2(double y, double x) {
    if (x == 0.0) {
        if (y > 0.0) return M_PI_2;
        if (y < 0.0) return -M_PI_2;
        return 0.0;
    }
    double atan = y / x;
    // Aproximare simplă bazată pe polinomiale
    if (x > 0.0) {
        return atan / (1.0 + 0.28 * atan * atan);
    } else if (y >= 0.0) {
        return atan / (1.0 + 0.28 * atan * atan) + M_PI;
    } else {
        return atan / (1.0 + 0.28 * atan * atan) - M_PI;
    }
}

double asin(double x) {
    if (x < -1.0 || x > 1.0) return NAN;
    return atan2(x, sqrt(1.0 - x * x));
}

double acos(double x) {
    if (x < -1.0 || x > 1.0) return NAN;
    return M_PI_2 - asin(x);
}


double log(double x) {
    if (x <= 0.0) return NAN;
    // Aproximare simplă pentru logaritm natural
    double val = (x - 1.0) / (x + 1.0);
    double val2 = val * val;
    double sum = 0.0;
    double denom = 1.0;
    for (int i = 1; i < 30; i += 2) {
        sum += val / denom;
        val *= val2;
        denom += 2.0;
    }
    return 2.0 * sum;
}

double log10(double x) {
    return log(x) / 2.30258509299404568402; // ln(10)
}

// Puteri și exponențiale elementare (bazate pe logaritmi și exp)
double exp(double x) {
    // e^x = limită (1 + x/n)^n, aproximare prin serie Taylor
    double sum = 1.0;
    double term = 1.0;
    for (int i = 1; i < 20; i++) {
        term *= x / i;
        sum += term;
    }
    return sum;
}

double pow(double base, double exponent) {
    if (exponent == 0.0) return 1.0;
    if (base == 0.0) return 0.0;
    if (base < 0.0) {
        return exp(exponent * log(-base));
    }
    return exp(exponent * log(base));
}

double ldexp(double x, int exp) {
    return x * pow(2.0, (double)exp);
}

double frexp(double value, int *exp) {
    if (!exp) return value;
    if (value == 0.0) {
        *exp = 0;
        return 0.0;
    }
    int e = 0;
    double abs_val = fabs(value);
    while (abs_val >= 1.0) {
        abs_val /= 2.0;
        e++;
    }
    while (abs_val < 0.5) {
        abs_val *= 2.0;
        e--;
    }
    *exp = e;
    return (value < 0.0) ? -abs_val : abs_val;
}

// ====================================================================
// HACK-URI DE SECURITATE ȘI COMPATIBILITATE GCC (_chk & C23)
// ====================================================================

int __printf_chk(int flag, const char *format, ...) {
    (void)flag;
    va_list args;
    va_start(args, format);
    int result = vfprintf(stdout, format, args);
    va_end(args);
    return result;
}

int __fprintf_chk(void *stream, int flag, const char *format, ...) {
    (void)flag;
    va_list args;
    va_start(args, format);
    int result = vfprintf((FILE*)stream, format, args);
    va_end(args);
    return result;
}

int __snprintf_chk(char *s, size_t maxlen, int flag, size_t os, const char *format, ...) {
    (void)flag; (void)os;
    va_list args;
    va_start(args, format);
    int result = vsnprintf(s, maxlen, format, args);
    va_end(args);
    return result;
}

int __vfprintf_chk(void *stream, int flag, const char *format, va_list args) {
    (void)flag;
    return vfprintf((FILE*)stream, format, args);
}

int __vsnprintf_chk(char *s, size_t size, int flag, size_t os, const char *format, va_list args) {
    (void)flag; (void)os;
    return vsnprintf(s, size, format, args);
}

char *__strncpy_chk(char *dest, const char *src, size_t n, size_t destlen) {
    (void)destlen;
    return strncpy(dest, src, n);
}

// Wrapper-e pentru C23 strtol / sscanf
long int __isoc23_strtol(const char *nptr, char **endptr, int base) {
    return strtol(nptr, endptr, base);
}



// Tabele locale GCC / Glibc
const unsigned short **__ctype_b_loc(void) {
    static const unsigned short *b = NULL;
    return &b;
}

const int **__ctype_toupper_loc(void) {
    static const int *t = NULL;
    return &t;
}

// ====================================================================
// PARSARE NUMERE (Suport pentru strtol și sscanf)
// ====================================================================

static int libc_digit_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return -1;
}

static long int libc_parse_long(const char *nptr, char **endptr, int base) {
    const char *start = nptr;
    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n' ||
           *nptr == '\r' || *nptr == '\f' || *nptr == '\v') nptr++;

    int negative = 0;
    if (*nptr == '-' || *nptr == '+') {
        negative = *nptr == '-';
        nptr++;
    }

    if (base == 0) {
        if (nptr[0] == '0' && (nptr[1] == 'x' || nptr[1] == 'X') &&
            libc_digit_value(nptr[2]) >= 0 && libc_digit_value(nptr[2]) < 16) {
            base = 16;
            nptr += 2;
        } else {
            base = nptr[0] == '0' ? 8 : 10;
        }
    } else if (base == 16 && nptr[0] == '0' &&
               (nptr[1] == 'x' || nptr[1] == 'X') &&
               libc_digit_value(nptr[2]) >= 0 && libc_digit_value(nptr[2]) < 16) {
        nptr += 2;
    }

    if (base < 2 || base > 36) {
        if (endptr) *endptr = (char *)start;
        return 0;
    }

    unsigned long value = 0;
    unsigned long limit = (~0UL >> 1) + (unsigned long)negative;
    int converted = 0;
    for (;;) {
        int digit = libc_digit_value(*nptr);
        if (digit < 0 || digit >= base) break;
        converted = 1;
        if (value <= (limit - (unsigned long)digit) / (unsigned long)base) {
            value = value * (unsigned long)base + (unsigned long)digit;
        } else {
            value = limit;
        }
        nptr++;
    }

    if (!converted) {
        if (endptr) *endptr = (char *)start;
        return 0;
    }

    if (endptr) *endptr = (char *)nptr;
    if (negative) {
        if (value == ((~0UL >> 1) + 1UL)) return -(long int)(~0UL >> 1) - 1;
        return -(long int)value;
    }
    return (long int)value;
}

// Funcția publică strtol cerută de linker
long int strtol(const char *nptr, char **endptr, int base) {
    return libc_parse_long(nptr, endptr, base);
}



// Corectare avertisment parametru nefolosit pentru __isoc23_sscanf
// Corectare pentru __isoc23_sscanf
int __isoc23_sscanf(const char *str, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = libc_vsscanf(str, format, args);
    va_end(args);
    return result;
}

void abort(void) {
    fprintf(stderr, "Abort called! Programul se opreste.\n");
    exit(1);
}

// ====================================================================
// STRNOD & STRPBRK (Compatibilitate suplimentară pentru Lua)
// ====================================================================

double strtod(const char *nptr, char **endptr) {
    if (!nptr) {
        if (endptr) *endptr = NULL;
        return 0.0;
    }

    const char *start = nptr;
    const char *p = nptr;
    while (isspace((unsigned char)*p)) {
        p++;
    }

    int sign = 1;
    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }

    double int_part = 0.0;
    int converted = 0;
    while (*p >= '0' && *p <= '9') {
        int_part = int_part * 10.0 + (*p - '0');
        p++;
        converted = 1;
    }

    double frac_part = 0.0;
    if (*p == '.') {
        p++;
        double divisor = 10.0;
        while (*p >= '0' && *p <= '9') {
            frac_part += (*p - '0') / divisor;
            divisor *= 10.0;
            p++;
            converted = 1;
        }
    }

    if (!converted) {
        if (endptr) *endptr = (char *)start;
        return 0.0;
    }

    double value = int_part + frac_part;
    if (*p == 'e' || *p == 'E') {
        const char *exponent_start = p;
        p++;
        int exponent_sign = 1;
        if (*p == '-' || *p == '+') {
            if (*p == '-') exponent_sign = -1;
            p++;
        }
        if (*p >= '0' && *p <= '9') {
            unsigned int exponent = 0;
            while (*p >= '0' && *p <= '9') {
                if (exponent < 1000) exponent = exponent * 10 + (unsigned int)(*p - '0');
                p++;
            }
            if (exponent > 400) exponent = 400;
            while (exponent--) {
                value = exponent_sign > 0 ? value * 10.0 : value / 10.0;
            }
        } else {
            p = exponent_start;
        }
    }

    if (endptr) {
        *endptr = (char *)p;
    }

    return sign * value;
}

char *strpbrk(const char *s, const char *accept) {
    if (!s || !accept) return NULL;
    while (*s) {
        const char *a = accept;
        while (*a) {
            if (*s == *a) {
                return (char *)s;
            }
            a++;
        }
        s++;
    }
    return NULL;
}

int strcoll(const char *s1, const char *s2) {
    return strcmp(s1, s2);
}

// ====================================================================
// FUNCȚII I/O SUPLIMENTARE PENTRU FILE STREAM-URI (Lua)
// ====================================================================

int feof(FILE *stream) {
    if (!stream) return 1;
    // Dacă ai un flag de EOF în structura ta FILE, returnează-l. 
    // Altfel, o verificare simplă sau un stub bazat pe starea stream-ului:
    return (stream->flags & 0x10) != 0; // Adaptează în funcție de structura ta FILE, sau return 0 dacă e minimal
}

int ferror(FILE *stream) {
    if (!stream) return 1;
    return (stream->flags & 0x20) != 0;
}

int getc(FILE *stream) {
    return fgetc(stream);
}

FILE *freopen(const char *filename, const char *mode, FILE *stream) {
    if (stream) {
        fclose(stream);
    }
    return fopen(filename, mode);
}


// ====================================================================
// FGETS (Citire linii din stream)
// ====================================================================

char *fgets(char *s, int size, FILE *stream) {
    if (!s || !stream || size <= 0) return NULL;
    if (size == 1) {
        s[0] = '\0';
        return s;
    }

    int count = 0;
    while (count < size - 1) {
        int c = fgetc(stream);
        if (c == EOF) break;

        if (stream == stdin && (c == '\b' || c == 0x7f)) {
            if (count > 0) {
                count--;
                fputs("\b", stdout);
            }
        } else if (stream == stdin && (c == '\r' || c == '\n')) {
            char nl[2] = {'\n', 0};
            fputs(nl, stdout);
            s[count++] = '\n';
            break;
        } else {
            if (stream == stdin) {
                char buf[2] = {(char)c, 0};
                fputs(buf, stdout);
            }
            s[count++] = (char)c;
            if (c == '\n') break;
        }
    }
    
    s[count] = '\0';
    return (count > 0 || size == 1) ? s : NULL;
}

int fgetc(FILE *stream) {
    if (!stream) return EOF;

    if (stream == stdin) {
        int c = 0;
        while (c == 0) c = nano_read_char();
        return c;
    }

    unsigned char c;
    ssize_t bytes = read(stream->fd, &c, 1);
    if (bytes < 0) {
        stream->flags |= 0x20;
        return EOF;
    }
    if (bytes == 0) {
        stream->flags |= 0x10;
        return EOF;
    }
    stream->offset++;
    return c;
}

// ====================================================================
// FUNCȚII I/O AVANSATE (Lua liolib)
// ====================================================================

FILE *tmpfile(void) {
    // Nano OS nu are un director temporar dedicat momentan; returnăm NULL
    return NULL;
}

int ungetc(int c, FILE *stream) {
    if (c == EOF || !stream) return EOF;
    return EOF;
}

void clearerr(FILE *stream) {
    if (stream) stream->flags &= ~(0x10 | 0x20);
}

int setvbuf(FILE *stream, char *buf, int mode, size_t size) {
    (void)buf; 
    (void)mode; 
    (void)size;
    if (!stream) return -1;
    return 0; // Succes fictiv pentru compatibilitate
}

// ====================================================================
// LOG2 (Compatibilitate Lua mathlib)
// ====================================================================

double log2(double x) {
    return log(x) / 0.693147180559945309417; // ln(x) / ln(2)
}

// ====================================================================
// TMPNAM (Stub pentru fișiere temporare)
// ====================================================================

char *tmpnam(char *s) {
    static char tmp_buf[L_tmpnam];
    char *target = s ? s : tmp_buf;
    // Generăm un nume simplu de fișier temporar
    sprintf(target, "/tmp/lua_%u", (unsigned int)(rand() % 10000));
    return target;
}

// ====================================================================
// COMPLETĂRI TIME.H (strftime, mktime, difftime)
// ====================================================================

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm) {
    if (!s || max == 0 || !format || !tm) return 0;
    
    // O implementare simplificată de bază pentru formatele comune folosite de Lua
    size_t written = 0;
    while (*format && written < max - 1) {
        if (*format == '%') {
            format++;
            char buf[16] = "";
            switch (*format) {
                case 'Y': sprintf(buf, "%04d", 1900 + tm->tm_year); break;
                case 'm': sprintf(buf, "%02d", tm->tm_mon + 1); break;
                case 'd': sprintf(buf, "%02d", tm->tm_mday); break;
                case 'H': sprintf(buf, "%02d", tm->tm_hour); break;
                case 'M': sprintf(buf, "%02d", tm->tm_min); break;
                case 'S': sprintf(buf, "%02d", tm->tm_sec); break;
                case '%': sprintf(buf, "%%"); break;
                default: buf[0] = *format; buf[1] = '\0'; break;
            }
            char *b = buf;
            while (*b && written < max - 1) {
                s[written++] = *b++;
            }
        } else {
            s[written++] = *format;
        }
        format++;
    }
    s[written] = '\0';
    return written;
}

time_t mktime(struct tm *timeptr) {
    if (!timeptr) return (time_t)-1;
    // Calcul simplificat bazat pe anii scurși (aproximare sigură pentru freestanding)
    int year = 1900 + timeptr->tm_year;
    time_t days = (year - 1970) * 365 + (year - 1969) / 4;
    days += timeptr->tm_mday;
    return days * 86400 + timeptr->tm_hour * 3600 + timeptr->tm_min * 60 + timeptr->tm_sec;
}

double difftime(time_t time1, time_t time0) {
    return (double)(time1 - time0);
}

// ====================================================================
// COMPLETĂRI CTYPE.H (iscntrl, isgraph)
// ====================================================================

int iscntrl(int c) {
    return (c >= 0 && c <= 31) || (c == 127);
}

int isgraph(int c) {
    return (c > 32 && c <= 126);
}

// ====================================================================
// FPUTS (Scriere șir în stream)
// ====================================================================

int fputs(const char *s, FILE *stream) {
    if (!s || !stream) return -1;
    size_t len = strlen(s);
    size_t written = fwrite(s, 1, len, stream);
    return (written == len) ? 0 : -1;
}


// ====================================================================
// FUNCȚII DE BAZĂ STRING & MEMORY (Lipsă la linkare)
// ====================================================================

size_t strlen(const char *s) {
    size_t len = 0;
    while (s && s[len]) len++;
    return len;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    for (size_t i = 0; i < n; i++) p[i] = (unsigned char)c;
    return s;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = s1, *p2 = s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) return p1[i] - p2[i];
    }
    return 0;
}

size_t strspn(const char *s, const char *accept) {
    size_t count = 0;
    while (*s) {
        const char *a = accept;
        int found = 0;
        while (*a) {
            if (*s == *a) { found = 1; break; }
            a++;
        }
        if (!found) break;
        count++;
        s++;
    }
    return count;
}

// ====================================================================
// STUBS PENTRU LOCALE, SIGNAL ȘI SETJMP
// ====================================================================
// ====================================================================
// STRUCTURA LCONV ȘI LOCALE (Compatibilitate Lua)
// ====================================================================

struct lconv {
    char *decimal_point;
    char *thousands_sep;
    char *grouping;
    char *int_curr_symbol;
    char *currency_symbol;
    char *mon_decimal_point;
    char *mon_thousands_sep;
    char *mon_grouping;
    char *positive_sign;
    char *negative_sign;
    char int_frac_digits;
    char frac_digits;
    char p_cs_precedes;
    char p_sep_by_space;
    char n_cs_precedes;
    char n_sep_by_space;
    char p_sign_posn;
    char n_sign_posn;
};

char *setlocale(int category, const char *locale) {
    (void)category;
    return (char *)locale;
}

struct lconv *localeconv(void) {
    static struct lconv lc = {
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
        .int_frac_digits = 127,
        .frac_digits = 127,
        .p_cs_precedes = 127,
        .p_sep_by_space = 127,
        .n_cs_precedes = 127,
        .n_sep_by_space = 127,
        .p_sign_posn = 127,
        .n_sign_posn = 127
    };
    return &lc;
}

typedef void (*sighandler_t)(int);
sighandler_t __sysv_signal(int signum, sighandler_t handler) {
    (void)signum; (void)handler;
    return NULL;
}

__asm__(
    ".text\n"
    ".global _setjmp\n"
    ".type _setjmp,@function\n"
    "_setjmp:\n"
    "movq %rbx, 0(%rdi)\n"
    "movq %rbp, 8(%rdi)\n"
    "movq %r12, 16(%rdi)\n"
    "movq %r13, 24(%rdi)\n"
    "movq %r14, 32(%rdi)\n"
    "movq %r15, 40(%rdi)\n"
    "leaq 8(%rsp), %rax\n"
    "movq %rax, 48(%rdi)\n"
    "movq (%rsp), %rax\n"
    "movq %rax, 56(%rdi)\n"
    "xorl %eax, %eax\n"
    "ret\n"
    ".size _setjmp, .-_setjmp\n"
    "\n"
    ".global __longjmp_chk\n"
    ".type __longjmp_chk,@function\n"
    "__longjmp_chk:\n"
    "movq %rdi, %rdx\n"
    "movl %esi, %eax\n"
    "testl %eax, %eax\n"
    "jnz 1f\n"
    "movl $1, %eax\n"
    "1:\n"
    "movq 48(%rdx), %rcx\n"
    "movq 56(%rdx), %r8\n"
    "movq 0(%rdx), %rbx\n"
    "movq 8(%rdx), %rbp\n"
    "movq 16(%rdx), %r12\n"
    "movq 24(%rdx), %r13\n"
    "movq 32(%rdx), %r14\n"
    "movq 40(%rdx), %r15\n"
    "movq %rcx, %rsp\n"
    "jmp *%r8\n"
    ".size __longjmp_chk, .-__longjmp_chk\n"
);


// ====================================================================
// STUBS ȘI FUNCȚII NOI PENTRU KILO
// ====================================================================

// 1. atexit - Înregistrarea unei funcții de ieșire (stub simplu sau gol)
int atexit(void (*function)(void)) {
    (void)function;
    return 0; // Succes fictiv
}

// 2. perror - Afișarea unei erori pe stderr
void perror(const char *s) {
    if (s) {
        fprintf(stderr, "%s: error\n", s);
    } else {
        fprintf(stderr, "error\n");
    }
}

// 3. getline - Citirea unei linii d dintr-un flux (implementare simplă peste fgets)
ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    if (!lineptr || !n || !stream) return -1;

    // Dacă bufferul nu este alocat, alocăm unul de bază (ex: 128 bytes)
    if (*lineptr == NULL || *n == 0) {
        *n = 128;
        *lineptr = (char*)malloc(*n);
        if (*lineptr == NULL) return -1;
    }

    size_t i = 0;
    int c;
    while ((c = fgetc(stream)) != EOF) {
        // Dacă nu mai avem loc în buffer, îl mărim (realloc)
        if (i >= *n - 1) {
            size_t new_size = *n * 2;
            char *new_ptr = (char*)realloc(*lineptr, new_size);
            if (!new_ptr) return -1;
            *lineptr = new_ptr;
            *n = new_size;
        }

        (*lineptr)[i++] = (char)c;
        if (c == '\n') break;
    }

    if (i == 0 && c == EOF) return -1;

    (*lineptr)[i] = '\0';
    return (ssize_t)i;
}

// ====================================================================
// STUBS POSIX PENTRU CONTROLUL TERMINALULUI ȘI KILO
// ====================================================================

int isatty(int fd) {
    // Returnăm 1 pentru stdin/stdout/stderr (descriptori 0, 1, 2)
    return (fd >= 0 && fd <= 2) ? 1 : 0;
}

int tcgetattr(int fd, struct termios *termios_p) {
    (void)fd; (void)termios_p;
    return 0; // Succes fictiv
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    (void)fd; (void)optional_actions; (void)termios_p;
    return 0; // Succes fictiv (deoarece Nano OS citește oricum brut prin SYSCALL_READ_CHAR)
}

int ioctl(int d, unsigned long request, ...) {
    (void)d; (void)request;
    return -1; // Indică eșec pentru ca Kilo să folosească dimensiunea default a ecranului
}

int ftruncate(int fd, long length) {
    (void)fd; 
    (void)length;
    return -1;
}