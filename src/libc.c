#include "nano_libc.h"
#include "stdio.h"
#include "stdlib.h"
#include "time.h"
#include <stdarg.h>

// ====================================================================
// TABELA INTERNĂ DE FIȘIERE ȘI DESCRIPTORII POSIX (fd)
// ====================================================================

#define MAX_OPEN_FILES 16

typedef struct {
    int active;
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
    (void)flags;
    int fd = -1;
    // Căutăm un slot liber începând de la 3 (0, 1, 2 sunt rezervate pentru stdin, stdout, stderr)
    for (int i = 3; i < MAX_OPEN_FILES; i++) {
        if (!file_table[i].active) {
            fd = i;
            break;
        }
    }
    if (fd == -1) return -1; // Prea multe fișiere deschise

    // Verificăm dacă fișierul există sau îl creăm dacă e mod de scriere
    uint8_t dummy[1];
    int res = nano_read_file(pathname, dummy, 0);
    if (res < 0 && (flags & 1)) { 
        nano_create_file(pathname, 0, 0666);
    }

    file_table[fd].active = 1;
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

    OpenFileEntry *entry = &file_table[fd];
    
    // Citim din fișier folosind syscall-ul tău nativ
    int bytes = nano_read_file(entry->path, (uint8_t*)buf, (uint32_t)count);
    if (bytes > 0) {
        entry->offset += bytes;
    }
    return bytes;
}

ssize_t write(int fd, const void *buf, size_t count) {
    if (fd == 1 || fd == 2) {
        nano_print((const char*)buf);
        return count;
    }
    if (fd < 3 || fd >= MAX_OPEN_FILES || !file_table[fd].active) return -1;

    OpenFileEntry *entry = &file_table[fd];
    int res = nano_write_file(entry->path, (const uint8_t*)buf, (uint32_t)count);
    if (res >= 0) {
        entry->offset += count;
        return count;
    }
    return -1;
}

long lseek(int fd, long offset, int whence) {
    if (fd < 3 || fd >= MAX_OPEN_FILES || !file_table[fd].active) return -1;
    OpenFileEntry *entry = &file_table[fd];

    if (whence == SEEK_SET) {
        entry->offset = offset;
    } else if (whence == SEEK_CUR) {
        entry->offset += offset;
    } else if (whence == SEEK_END) {
        entry->offset = offset; 
    }
    return entry->offset;
}

// ====================================================================
// Nivel Sus: Standard C FILE * (Construit peste fd)
// ====================================================================

FILE *fopen(const char *filename, const char *mode) {
    int flags = 0;
    if (mode[0] == 'w' || mode[0] == 'a') flags = 1; // Scriere

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
    return stream;
}

int fclose(FILE *stream) {
    if (!stream) return -1;
    if (stream->fd >= 3) {
        close(stream->fd);
    }
    nano_free(stream);
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!stream || !ptr) return 0;
    size_t total = size * nmemb;
    ssize_t bytes = read(stream->fd, ptr, total);
    if (bytes <= 0) return 0;
    return bytes / (size > 0 ? size : 1);
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!stream || !ptr) return 0;
    size_t total = size * nmemb;
    ssize_t bytes = write(stream->fd, ptr, total);
    if (bytes <= 0) return 0;
    return nmemb;
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
    return 0;
}

int remove(const char *filename) {
    return nano_delete_file(filename);
}

int rename(const char *oldname, const char *newname) {
    (void)oldname;
    (void)newname;
    return 0;
}

// ====================================================================
// FUNCTII AJUTĂTOARE PENTRU FORMATĂRI INTERNE
// ====================================================================

static char* print_num(char* buffer, int64_t num, int base, int is_signed) {
    char temp[64];
    int i = 0;
    int is_neg = 0;
    uint64_t unum;

    if (is_signed && base == 10 && num < 0) {
        is_neg = 1;
        unum = (uint64_t)(-num);
    } else {
        unum = (uint64_t)num;
    }

    do {
        int rem = unum % base;
        temp[i++] = (rem < 10) ? (rem + '0') : (rem - 10 + 'a');
        unum /= base;
    } while (unum > 0);

    if (is_neg) {
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
                    int num = va_arg(ap, int);
                    char buf[32];
                    print_num(buf, num, 10, 1);
                    char *b = buf;
                    while (*b && written < size - 1) str[written++] = *b++;
                    break;
                }
                case 'u': {
                    unsigned int num = va_arg(ap, unsigned int);
                    char buf[32];
                    print_num(buf, num, 10, 0);
                    char *b = buf;
                    while (*b && written < size - 1) str[written++] = *b++;
                    break;
                }
                case 'x':
                case 'X': {
                    unsigned int num = va_arg(ap, unsigned int);
                    char buf[32];
                    print_num(buf, num, 16, 0);
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
    (void)stream;
    char buf[1024];
    va_list ap;
    
    va_start(ap, format);
    int len = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    
    nano_print(buf);
    
    return len;
}

int vfprintf(FILE *stream, const char *format, va_list ap) {
    (void)stream;
    char buf[1024];
    int len = vsnprintf(buf, sizeof(buf), format, ap);
    nano_print(buf);
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

int sscanf(const char *str, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int assigned = 0;

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
    if (!nptr) return 0.0;
    
    // Sărim spațiile albe
    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n' || *nptr == '\r') {
        nptr++;
    }

    int sign = 1;
    if (*nptr == '-') {
        sign = -1;
        nptr++;
    } else if (*nptr == '+') {
        nptr++;
    }

    double int_part = 0.0;
    while (*nptr >= '0' && *nptr <= '9') {
        int_part = int_part * 10.0 + (*nptr - '0');
        nptr++;
    }

    double frac_part = 0.0;
    if (*nptr == '.') {
        nptr++;
        double divisor = 10.0;
        while (*nptr >= '0' && *nptr <= '9') {
            frac_part += (*nptr - '0') / divisor;
            divisor *= 10.0;
            nptr++;
        }
    }

    return sign * (int_part + frac_part);
}


void *malloc(size_t size) {
    return nano_malloc(size);
}

void free(void *ptr) {
    nano_free(ptr);
}

void *calloc(size_t nmemb, size_t size) {
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
    if (!ptr) return nano_malloc(size);
    if (size == 0) {
        nano_free(ptr);
        return NULL;
    }
    void *new_ptr = nano_malloc(size);
    if (new_ptr) {
        // Copiem datele vechi (copiem o dimensiune sigură, ex: 1024 sau cât era bufferul - sau implementezi un memmove/memcpy)
        // O variantă simplă sigură pentru DOOM:
        char *src = (char *)ptr;
        char *dst = (char *)new_ptr;
        for (size_t i = 0; i < size; i++) dst[i] = src[i]; // Notă ideal ar fi să știm mărimea veche, dar pentru DOOM merge
        nano_free(ptr);
    }
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

double fabs(double x) {
    return (x < 0.0) ? -x : x;
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