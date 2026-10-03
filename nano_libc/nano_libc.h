#ifndef NANO_LIBC_H
#define NANO_LIBC_H

#include <stdint.h>
#include <stddef.h>  // Pentru size_t
#include <stdarg.h> // Necesar pentru sprintf dacă folosește argumente variabile

#include "../kernel/syscall.h"



// 1. Definim structura exact cum este ea în kernel
typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
} DateTime;

// structura proces table:
typedef struct {
    uint32_t pid;
    uint32_t ppid;
    uint32_t state;
    char name[32];
} ProcessInfo;

static inline int nano_cd(const char* path) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_CD), "D"(path)
        : "memory"
    );
    return ret;
}

/*
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
*/

static inline int nano_exec(const char* filename) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" ((uint64_t)SYSCALL_EXEC), "D" ((uint64_t)filename) 
        : "cc", "memory"
    );
    return (int)ret;
}

static inline void nano_shutdown(void) {
    __asm__ volatile (
        "int $0x80"
        :
        : "a" ((uint64_t)SYSCALL_SHUTDOWN)
        : "cc", "memory"
    );
}

static inline void nano_ls(const char* path) {
    __asm__ volatile (
        "int $0x80"
        : 
        : "a"(SYSCALL_LIST_FILES), "D"(path)  // Folosim "D" care reprezintă registrul RDI
        : "memory"
    );
}

static inline int nano_create_file(const char* name, uint32_t size) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" ((uint64_t)10), "D" ((uint64_t)name), "S" ((uint64_t)size)
        : "cc", "memory"
    );
    return (int)ret;
}

static inline int nano_write_file(const char* name, const uint8_t* data, uint32_t size) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" ((uint64_t)11), "D" ((uint64_t)name), "S" ((uint64_t)data), "d" ((uint64_t)size)
        : "cc", "memory"
    );
    return (int)ret;
}

static inline void nano_format(void) {
    __asm__ volatile (
        "int $0x80"
        :
        : "a" ((uint64_t)12)
        : "cc", "memory"
    );
}

static inline int nano_delete_file(const char* name) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" ((uint64_t)13), "D" ((uint64_t)name)
        : "cc", "memory"
    );
    return (int)ret;
}

static inline int nano_read_file(const char* name, uint8_t* buffer, uint32_t max_size) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" ((uint64_t)14), "D" ((uint64_t)name), "S" ((uint64_t)buffer), "d" ((uint64_t)max_size)
        : "cc", "memory"
    );
    return (int)ret;
}

static inline int nano_mkdir(const char* name) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" ((uint64_t)15), "D" ((uint64_t)name)
        : "cc", "memory"
    );
    return (int)ret;
}

static inline void nano_print_int(uint32_t val) {
    __asm__ volatile (
        "int $0x80"
        : 
        : "a" ((uint64_t)16), "D" ((uint64_t)val) // RAX = 16 (SYSCALL_PRINT_INT), RDI = valoarea
        : "cc", "memory"
    );
}

static inline char nano_read_char(void) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" ((uint64_t)17) // RAX = 17 (SYSCALL_READ_CHAR)
        : "cc", "memory"
    );
    return (char)ret;
}

static inline void nano_clear_screen(void) {
    __asm__ volatile (
        "int $0x80"
        : 
        : "a" ((uint64_t)18) // SYSCALL_CLEAR_SCREEN
        : "cc", "memory"
    );
}

static inline void nano_pwd(char* buffer, uint32_t max_len) {
    __asm__ volatile (
        "int $0x80"
        : 
        : "a"(SYSCALL_PWD), "D"(buffer), "S"(max_len)  // D = rdi (buffer), S = rsi (max_len)
        : "memory"
    );
}


static inline void nano_sys_exit(int status) {
    // Trimităm codul 20 (SYSCALL_EXIT) în RAX, iar status-ul în RDI
    __asm__ volatile (
        "mov %1, %%rax\n\t"
        "mov %0, %%rdi\n\t"
        "int $0x80"
        : 
        : "r"((uint64_t)status), "r"((uint64_t)SYSCALL_EXIT)
        : "rax", "rdi", "memory"
    );
    while(1);
}



void nano_print(const char* str);
static inline void newline(){
	nano_print("\n");
}
void nano_readline(char* buffer, uint32_t max_len);

void* nano_malloc(size_t size);
void nano_free(void* ptr); // (Chiar dacă e bump allocator, e bine să avem interfața)

void* memset(void* dest, int val, size_t len);
void* memcpy(void* dest, const void* src, size_t len);
void* memmove(void* dest, const void* src, size_t n);

uint32_t read_file_to_buffer(const char* path, uint8_t** out_buffer);

size_t strlen(const char* str);
int strcmp(const char* s1, const char* s2);
char* strcpy(char* dest, const char* src);
int starts_with(const char* text, const char* prefix);

// Conversii
int atoi(const char* str);
char* itoa(int value, char* str, int base);

void sleep(uint32_t milliseconds);

// 2. Funcția pe care o va apela programul tău
void get_time(DateTime* dt);

int nano_get_processes(ProcessInfo* buf, int max_entries);
int nano_wait();
int nano_kill(int pid);





static inline char* nano_strcat(char* dest, const char* src) {
    char* ptr = dest + strlen(dest);
    while (*src != '\0') {
        *ptr++ = *src++;
    }
    *ptr = '\0';
    return dest;
}




// Helper pentru sprintf (suportă de bază formatul %d și %s folosite în editor)
static inline int sprintf(char* str, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int i = 0;
    char* ptr = str;
    
    while (format[i] != '\0') {
        if (format[i] == '%' && format[i+1] == 'd') {
            int val = va_arg(args, int);
            char buf[16];
            itoa(val, buf, 10);
            char* b = buf;
            while (*b) *ptr++ = *b++;
            i += 2;
        } else if (format[i] == '%' && format[i+1] == 's') {
            char* s = va_arg(args, char*);
            while (*s) *ptr++ = *s++;
            i += 2;
        } else if (format[i] == '%' && format[i+1] == 'c') {
            char c = (char)va_arg(args, int);
            *ptr++ = c;
            i += 2;
        } else {
            *ptr++ = format[i++];
        }
    }
    *ptr = '\0';
    va_end(args);
    return (int)(ptr - str);
}

int nano_get_meminfo(MemInfo* info);

int nano_getcwd(char* buf, int max_len); //curent dir

char* string_copy(char* dest, const char* src);

int64_t parse_int64(const char* str);
//float parse_float(const char* str);
//float nano_parse_float(const char* str);

// Trimite un mesaj în syslog-ul kernelului

// Și rescrie nano_syslog și nano_getlog curat, fără mov-uri manuale paralele:
static inline void nano_syslog(const char* msg) {
    __asm__ volatile (
        "int $0x80"
        : : "a"((uint64_t)SYSCALL_SYSLOG), "D"((uint64_t)msg) : "memory"
    );
}

static inline void nano_getlog(char* dest, int max_len) {
    __asm__ volatile (
        "int $0x80"
        : : "a"((uint64_t)SYSCALL_GETLOG), "D"((uint64_t)dest), "S"((uint64_t)max_len) : "memory"
    );
}

static inline int nano_has_char(void) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" ((uint64_t)18) // SYSCALL_HAS_CHAR = 18
        : "cc", "memory"
    );
    return (int)ret;
}

static inline void nano_reboot() {
    __asm__ volatile (
        "mov $32, %%rax\n" // Numărul SYSCALL_REBOOT
        "int $0x80\n"      // Sau întreruperea ta de syscall
        :
        :
        : "rax"
    );
}

static inline void nano_beep(uint32_t frequency, uint32_t duration_ms) {
    __asm__ volatile (
        "mov $34, %%rax\n"         // Numărul syscall-ului pentru Beep (ex: 34)
        "mov %0, %%rdi\n"          // Primul argument: Frecvența devine %0
        "mov %1, %%rsi\n"          // Al doilea argument: Durata devine %1
        "int $0x80\n"
        :
        : "r"((uint64_t)frequency), "r"((uint64_t)duration_ms)
        : "rax", "rdi", "rsi", "memory"
    );
}

static inline void nano_set_cursor_shape(int style) {
    __asm__ volatile (
        "mov $35, %%rax\n"         // Numărul noului syscall (35)
        "mov %0, %%rdi\n"          // Stilul trimis în rdi (0 = bloc, 1 = underline)
        "int $0x80\n"
        :
        : "r"((uint64_t)style)
        : "rax", "rdi", "memory"
    );
}

// În user-space (ex: un fișier helper de syscall-uri)
static inline int sys_play_audio(const uint8_t* data, uint32_t length) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(36), "D"(data), "S"(length)
        : "memory"
    );
    return ret;
}

#endif
