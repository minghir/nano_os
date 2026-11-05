#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>
#include <stdarg.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

#define EOF (-1)


// --- DESCRIPTORI DE FIȘIERE POSIX (Nivel Jos) ---
typedef long ssize_t;

int open(const char *pathname, int flags, ...);
int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
long lseek(int fd, long offset, int whence);

// --- STRUCTURA FILE (Nivel Sus - Standard C) ---
typedef struct _iobuf {
    int fd;
    int flags;
    long offset;
} FILE;

// Fluxurile standard POSIX
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

// --- FUNCȚII STANDARD C (Formating & I/O) ---
int printf(const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
int fprintf(FILE *stream, const char *format, ...);
int vfprintf(FILE *stream, const char *format, va_list ap);

int fflush(FILE *stream);
int puts(const char *s);
int putchar(int c);
int sscanf(const char *str, const char *format, ...);
int scanf(const char *format, ...);

// --- FUNCȚII FIȘIERE (Construite peste open/read/write/close) ---
FILE *fopen(const char *filename, const char *mode);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
long ftell(FILE *stream);
int fgetc(FILE *stream);

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int fseek(FILE *stream, long offset, int whence);
int remove(const char *filename);
int rename(const char *oldname, const char *newname);

#ifndef BUFSIZ
#define BUFSIZ 8192
#endif

// Prototipuri noi pentru fluxuri
int feof(FILE *stream);
int ferror(FILE *stream);
int getc(FILE *stream);

char *fgets(char *s, int size, FILE *stream);

FILE *freopen(const char *filename, const char *mode, FILE *stream);

#define _IONBF 0
#define _IOFBF 1
#define _IOLBF 2

FILE *tmpfile(void);
int ungetc(int c, FILE *stream);
void clearerr(FILE *stream);
int setvbuf(FILE *stream, char *buf, int mode, size_t size);

#ifndef L_tmpnam
#define L_tmpnam 20
#endif

char *tmpnam(char *s);

int fputs(const char *s, FILE *stream);

#endif // _STDIO_H