#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>
#include <stdio.h>
#include "termios.h"


#ifndef NULL
#define NULL ((void*)0)
#endif

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 32767

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef long ssize_t;
#endif

// --- Gestiunea memoriei ---
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

// --- Conversii șiruri în numere ---
int atoi(const char *nptr);
long atol(const char *nptr);
double atof(const char *nptr);
double strtod(const char *nptr, char **endptr);
long int strtol(const char *nptr, char **endptr, int base);
// --- Numere aleatoare (RNG) ---
int rand(void);
void srand(unsigned int seed);

// --- Control proces / Mediu ---
void exit(int status);
char *getenv(const char *name);
int abs(int j);
long labs(long j);

int system(const char *command);
void abort(void);
int atexit(void (*function)(void));
void perror(const char *s);
ssize_t getline(char **lineptr, size_t *n, FILE *stream);



int isatty(int fd);
int tcgetattr(int fd, struct termios *termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);
int ioctl(int d, unsigned long request, ...);
int ftruncate(int fd, long length); // Folosim 'long' în loc de 'off_t' dacă off_t nu este definit

#endif // _STDLIB_H