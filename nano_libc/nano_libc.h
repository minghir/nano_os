#ifndef NANO_LIBC_H
#define NANO_LIBC_H

#include <stdint.h>
#include <stddef.h>  // Pentru size_t


#define SYSCALL_PRINT     1
#define SYSCALL_READLINE  2
#define SYSCALL_MALLOC    3
#define SYSCALL_SLEEP     4
#define SYSCALL_DATETIME  5

// 1. Definim structura exact cum este ea în kernel
typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
} DateTime;


static inline int nano_exec(const char* filename) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" ((uint64_t)6), "D" ((uint64_t)filename) // 6 este numărul pentru SYSCALL_EXEC
        : "cc", "memory"
    );
    return (int)ret;
}

static inline void nano_shutdown(void) {
    __asm__ volatile (
        "int $0x80"
        :
        : "a" ((uint64_t)7) // 7 este SYSCALL_SHUTDOWN
        : "cc", "memory"
    );
}

void nano_print(const char* str);
void nano_readline(char* buffer, uint32_t max_len);

void* nano_malloc(size_t size);
void nano_free(void* ptr); // (Chiar dacă e bump allocator, e bine să avem interfața)

void* memset(void* dest, int val, size_t len);
void* memcpy(void* dest, const void* src, size_t len);
size_t strlen(const char* str);
int strcmp(const char* s1, const char* s2);
char* strcpy(char* dest, const char* src);


// Conversii
int atoi(const char* str);
char* itoa(int value, char* str, int base);

void sleep(uint32_t milliseconds);

// 2. Funcția pe care o va apela programul tău
void get_time(DateTime* dt);

#endif