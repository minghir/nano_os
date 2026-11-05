#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 32767

// --- Gestiunea memoriei ---
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

// --- Conversii șiruri în numere ---
int atoi(const char *nptr);
long atol(const char *nptr);
double atof(const char *nptr);
long strtol(const char *nptr, char **endptr, int base);

// --- Numere aleatoare (RNG) ---
int rand(void);
void srand(unsigned int seed);

// --- Control proces / Mediu ---
void exit(int status);
char *getenv(const char *name);
int abs(int j);
long labs(long j);

int system(const char *command);

#endif // _STDLIB_H