#include "nano_libc.h"

void nano_print(const char* str) {
    // GCC știe nativ: "a" = RAX, "D" = RDI
    __asm__ volatile (
        "int $0x80"
        : /* Fără variabile de ieșire */
        : "a" ((uint64_t)1), "D" ((uint64_t)str)
        : "memory"
    );
}

void nano_readline(char* buffer, uint32_t max_len) {
    // GCC știe nativ: "a" = RAX, "D" = RDI, "S" = RSI
    __asm__ volatile (
        "int $0x80"
        : /* Fără variabile de ieșire */
        : "a" ((uint64_t)2), "D" ((uint64_t)buffer), "S" ((uint64_t)max_len)
        : "memory"
    );
}

void* nano_malloc(size_t size) {
    uint64_t ret_ptr;
    __asm__ volatile (
        "mov $3, %%rax \n"      // Comanda 3: Malloc
        "mov %1, %%rdi \n"      // Parametru: dimensiunea
        "int $0x80 \n"
        "mov %%rax, %0 \n"      // Salvăm rezultatul întors în RAX
        : "=r" (ret_ptr)
        : "r" ((uint64_t)size)
        : "rax", "rdi"
    );
    return (void*)ret_ptr;
}

void nano_free(void* ptr) {
    // Bump allocator nu face nimic la free, dar putem lăsa funcția goală
    (void)ptr;
}