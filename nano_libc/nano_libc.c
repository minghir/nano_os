#include "nano_libc.h"

void nano_print(const char* str) {
    // GCC știe nativ: "a" = RAX, "D" = RDI
    __asm__ volatile (
        "int $0x80"
        : /* Fără variabile de ieșire */
        : "a" ((uint64_t)SYSCALL_PRINT), "D" ((uint64_t)str)
        : "memory"
    );
}

void nano_readline(char* buffer, uint32_t max_len) {
    // GCC știe nativ: "a" = RAX, "D" = RDI, "S" = RSI
    __asm__ volatile (
        "int $0x80"
        : /* Fără variabile de ieșire */
        : "a" ((uint64_t)SYSCALL_READLINE), "D" ((uint64_t)buffer), "S" ((uint64_t)max_len)
        : "memory"
    );
}

void* nano_malloc(size_t size) {
    uint64_t ret;
    __asm__ volatile (
        "mov %1, %%rax\n\t"
        "mov %2, %%rdi\n\t"
        "int $0x80\n\t"        // Sau instrucțiunea ta de syscall (ex: syscall)
        "mov %%rax, %0\n\t"
        : "=r"(ret)
        : "r"((uint64_t)SYSCALL_MALLOC), "r"((uint64_t)size)
        : "%rax", "%rdi", "memory"
    );
    return (void*)ret;
}

// Wrapper user-space pentru free
void nano_free(void* ptr) {
    if (!ptr) return;
    __asm__ volatile (
        "mov %0, %%rax\n\t"
        "mov %1, %%rdi\n\t"
        "int $0x80\n\t"
        : 
        : "r"((uint64_t)SYSCALL_FREE), "r"((uint64_t)ptr)
        : "%rax", "%rdi", "memory"
    );
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
    __asm__ volatile (
        "int $0x80"
        : // Nu avem variabile de ieșire
        : "a" ((uint64_t)SYSCALL_SLEEP), "D" ((uint64_t)milliseconds) // "a" forțează RAX = 4, "D" forțează RDI = milliseconds
        : "cc", "memory"
    );
}

void get_time(DateTime* dt) {
    __asm__ volatile (
        "int $0x80"
        : 
        : "a" ((uint64_t)SYSCALL_DATETIME), "D" ((uint64_t)dt) // RAX = 5 (syscall nr 5), RDI = adresa structurii 'dt'
        : "cc", "memory"
    );
}

int starts_with(const char* text, const char* prefix) {
    while (*prefix) {
        if (*text++ != *prefix++) return 0;
    }
    return 1;
}


int nano_get_processes(ProcessInfo* buf, int max_entries) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" ((uint64_t)21), "D" ((uint64_t)buf), "S" ((uint64_t)max_entries)
        : "memory"
    );
    return (int)ret;
}

int nano_wait() {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" ((uint64_t)22) // Numărul syscall-ului SYSCALL_WAIT
        : "memory"
    );
    return (int)ret;
}

// în nano_libc.c / nano_libc.h
int nano_kill(int pid) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" ((uint64_t)SYSCALL_KILL), "D" ((uint64_t)pid)
        : "memory"
    );
    return (int)ret;
}

int nano_get_meminfo(MemInfo* info) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(24), "D"(info) // 24 este SYSCALL_MEMINFO, RDI = info
        : "memory"
    );
    return (int)ret;
}

int nano_getcwd(char* buf, int max_len) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(25), "D"(buf), "S"(max_len) // 25 = SYSCALL_GETCWD, RDI = buf, RSI = max_len
        : "memory"
    );
    return (int)ret;
}

char* string_copy(char* dest, const char* src) {
    char* original_dest = dest;
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0'; // Nu uităm terminatorul de șir!
    return original_dest;
}

// Funcție custom pentru a converti string la int64_t (fără dependențe de atoll din libc)
int64_t parse_int64(const char* str) {
    int64_t res = 0;
    int sign = 1;
    int i = 0;
    
    if (str[0] == '-') {
        sign = -1;
        i = 1;
    } else if (str[0] == '+') {
        i = 1;
    }
    
    while (str[i] >= '0' && str[i] <= '9') {
        res = res * 10 + (str[i] - '0');
        i++;
    }
    
    return res * sign;
}
/*
// Funcție pentru a converti un string zecimal (ex: "3.14") într-un float real
float parse_float(const char* str) {
    int64_t int_part = 0;
    int64_t frac_part = 0;
    int divisor = 1;
    int sign = 1;
    int i = 0;

    if (str[0] == '-') {
        sign = -1;
        i = 1;
    } else if (str[0] == '+') {
        i = 1;
    }

    // Partea întreagă
    while (str[i] >= '0' && str[i] <= '9') {
        int_part = int_part * 10 + (str[i] - '0');
        i++;
    }

    // Partea fracționară (după punct)
    if (str[i] == '.') {
        i++;
        while (str[i] >= '0' && str[i] <= '9') {
            frac_part = frac_part * 10 + (str[i] - '0');
            divisor *= 10;
            i++;
        }
    }

    float res = (float)int_part + ((float)frac_part / (float)divisor);
    return res * sign;
}

float nano_parse_float(const char* str) {
    float result = 0.0f;
    float sign = 1.0f;
    int i = 0;

    if (str[0] == '-') {
        sign = -1.0f;
        i++;
    } else if (str[0] == '+') {
        i++;
    }

    // Partea întreagă
    while (str[i] >= '0' && str[i] <= '9') {
        result = result * 10.0f + (str[i] - '0');
        i++;
    }

    // Partea fracționară (după virgulă/punct)
    if (str[i] == '.') {
        i++;
        float fraction = 1.0f;
        while (str[i] >= '0' && str[i] <= '9') {
            fraction /= 10.0f;
            result += (str[i] - '0') * fraction;
            i++;
        }
    }

    return result * sign;
}
*/


void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    if (d == s) return dest;
    if (d < s) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}


uint32_t read_file_to_buffer(const char* path, uint8_t** out_buffer) {
    // Alocăm un buffer generos pentru fișierul MP3 (ex: 2MB, cât maximul heap-ului sau cât permite fișierul)
    uint32_t max_size = 2 * 1024 * 1024; // 2 MB
    uint8_t* buf = (uint8_t*)nano_malloc(max_size);
    if (!buf) {
        return 0;
    }

    // Apelăm funcția din nano_libc.h (care folosește SYSCALL_READ_FILE / 14)
    int bytes_read = nano_read_file(path, buf, max_size);
    if (bytes_read <= 0) {
        nano_free(buf);
        *out_buffer = 0;
        return 0;
    }

    *out_buffer = buf;
    return (uint32_t)bytes_read;
}