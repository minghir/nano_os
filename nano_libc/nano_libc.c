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


// --- MEMORIE ---

void* memset(void* dest, int val, size_t len) {
    unsigned char* ptr = (unsigned char*)dest;
    for (size_t i = 0; i < len; i++) {
        ptr[i] = (unsigned char)val;
    }
    return dest;
}

void* memcpy(void* dest, const void* src, size_t len) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < len; i++) {
        d[i] = s[i];
    }
    return dest;
}

// --- STRING-URI ---

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

char* strcpy(char* dest, const char* src) {
    char* saved_dest = dest;
    while ((*dest++ = *src++) != '\0');
    return saved_dest;
}

// --- CONVERSII ---

int atoi(const char* str) {
    int res = 0;
    int sign = 1;
    int i = 0;

    // Ignorăm spațiile albe de la început
    while (str[i] == ' ' || str[i] == '\t' || str[i] == '\n' || str[i] == '\r') {
        i++;
    }

    // Gestionăm semnul
    if (str[i] == '-') {
        sign = -1;
        i++;
    } else if (str[i] == '+') {
        i++;
    }

    // Convertim cifrele
    while (str[i] >= '0' && str[i] <= '9') {
        res = res * 10 + (str[i] - '0');
        i++;
    }

    return res * sign;
}

char* itoa(int value, char* str, int base) {
    char* rc;
    char* ptr;
    char* low;
    // Doar bazele suportate uzual (de la 2 la 36)
    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }

    ptr = str;
    // Gestionăm numere negative doar pentru baza 10
    if (value < 0 && base == 10) {
        *ptr++ = '-';
        value = -value;
    }

    rc = ptr;

    // Generăm cifrele invers
    do {
        int t = value % base;
        if (t >= 10) {
            *ptr++ = 'a' + (t - 10);
        } else {
            *ptr++ = '0' + t;
        }
        value /= base;
    } while (value > 0);

    *ptr-- = '\0';

    // Inversăm șirul obținut pentru a fi în ordine corectă
    low = rc;
    while (low < ptr) {
        char tmp = *low;
        *low++ = *ptr;
        *ptr-- = tmp;
    }

    return str;
}

void sleep(uint32_t milliseconds) {
    // Exemplu bazat pe convenția ta (RAX = 4, RDI = milliseconds)
    __asm__ volatile (
        "mov $4, %%rax\n\t"
        "mov %0, %%rdi\n\t"
        "int $0x80\n\t" // Sau instrucțiunea ta de syscall (ex: 'syscall')
        :
        : "r" ((uint64_t)milliseconds)
        : "rax", "rdi", "cc", "memory"
    );
}