#ifndef NANO_TCC_COMPAT_H
#define NANO_TCC_COMPAT_H

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

// 1. Tipuri primitive POSIX (fără ssize_t, fiindcă vine din stdio.h)
typedef long ptrdiff_t;
typedef long off_t;
typedef int mode_t;

#define RTLD_DEFAULT ((void*)0)

#ifndef NULL
#define NULL ((void*)0)
#endif

#define EINTR   4
#define ENOENT  2
#define EIO     5
#define ENOMEM  12
#define EACCES  13
#define EINVAL  22

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

// Variabile de mediu
static char *nano_environ[] = { (void*)0 };
#define environ nano_environ

// 2. Prevenim conflictul is_space între nano_string.h și tcc.h
#define is_space nano_libc_is_space

// 3. Includem librăria Nano OS și headerele standard oficiale
#include "nano_libc.h"
#include "stdio.h"
#include "stdlib.h"

// Re-mapăm is_space pentru TCC
#undef is_space
#define is_space tcc_is_space

// (Funcțiile open, close, read, write, lseek sunt acum preluate direct din stdio.h / libc.c!)

static inline int unlink(const char* pathname) {
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

// 5. Manipulări de șiruri & Math stubs specifice TCC
static inline char* realpath(const char* path, char* resolved_path) {
    if (!path) return NULL;
    if (!resolved_path) {
        resolved_path = (char*)malloc(256);
    }
    if (resolved_path) {
        strcpy(resolved_path, path);
    }
    return resolved_path;
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

static inline char* strerror(int errnum) {
    (void)errnum;
    return "Unknown error";
}

static inline long double ldexpl(long double x, int exp) {
    (void)exp;
    return x;
}

static inline float strtof(const char* nptr, char** endptr) {
    if (endptr) *endptr = (char*)nptr;
    return 0.0f;
}

static inline long double strtold(const char* nptr, char** endptr) {
    if (endptr) *endptr = (char*)nptr;
    return 0.0L;
}

#endif