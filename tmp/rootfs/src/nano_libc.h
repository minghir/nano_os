#ifndef NANO_LIBC_H
#define NANO_LIBC_H

#include <stdint.h>
#include <stddef.h>  // Pentru size_t
#include <stdarg.h> // Necesar pentru sprintf dacă folosește argumente variabile

#include "../kernel/syscall.h"
#include "../kernel/video.h"
#include "../kernel/mouse.h"
#include "../kernel/keyboard.h"
#include "nano_string.h"

// Tipul de date pentru mutex (volatile ca să nu îl optimizeze compilatorul)
typedef volatile int nano_mutex_t;

// Inițializarea unui mutex (0 = deblocat, 1 = blocat)
#define NANO_MUTEX_INIT 0

// Funcția de BLOCARE (Lock)



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
	uint32_t uid;
    char name[32];
} ProcessInfo;

size_t strlen(const char* str);
int atoi(const char* str);
char* itoa(int value, char* str, int base);

static inline char* strcpy(char* dest, const char* src) {
    char* saved_dest = dest;
    while ((*dest++ = *src++) != '\0');
    return saved_dest;
}

// --- CONVERSII ---

static inline int64_t parse_int64(const char* str) {
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

static inline void* memmove(void* dest, const void* src, size_t n) {
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


static inline int nano_kill(int pid) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" ((uint64_t)SYSCALL_KILL), "D" ((uint64_t)pid)
        : "memory"
    );
    return (int)ret;
}

static inline int nano_get_meminfo(MemInfo* info) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(24), "D"(info) // 24 este SYSCALL_MEMINFO, RDI = info
        : "memory"
    );
    return (int)ret;
}

static inline int nano_getcwd(char* buf, int max_len) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(25), "D"(buf), "S"(max_len) // 25 = SYSCALL_GETCWD, RDI = buf, RSI = max_len
        : "memory"
    );
    return (int)ret;
}

static inline int nano_wait(void) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" ((uint64_t)SYSCALL_WAIT) // Folosim constanta din syscall.h!
        : "memory"
    );
    return (int)ret;
}

static inline int nano_get_processes(ProcessInfo* buf, int max_entries) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" ((uint64_t)21), "D" ((uint64_t)buf), "S" ((uint64_t)max_entries)
        : "memory"
    );
    return (int)ret;
}


static inline void get_time(DateTime* dt) {
    __asm__ volatile (
        "int $0x80"
        : 
        : "a" ((uint64_t)SYSCALL_DATETIME), "D" ((uint64_t)dt) // RAX = 5 (syscall nr 5), RDI = adresa structurii 'dt'
        : "cc", "memory"
    );
}


static inline void nano_free(void* ptr) {
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


static inline void* nano_malloc(size_t size) {
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


static inline void nano_readline(char* buffer, uint32_t max_len) {
    // GCC știe nativ: "a" = RAX, "D" = RDI, "S" = RSI
    __asm__ volatile (
        "int $0x80"
        : /* Fără variabile de ieșire */
        : "a" ((uint64_t)SYSCALL_READLINE), "D" ((uint64_t)buffer), "S" ((uint64_t)max_len)
        : "memory"
    );
}





static inline void nano_print(const char* str) {
    // GCC știe nativ: "a" = RAX, "D" = RDI
    __asm__ volatile (
        "int $0x80"
        : /* Fără variabile de ieșire */
        : "a" ((uint64_t)SYSCALL_PRINT), "D" ((uint64_t)str)
        : "memory"
    );
}


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

static inline uint32_t read_file_to_buffer(const char* path, uint8_t** out_buffer) {
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
        : "a" ((uint64_t)SYSCALL_READ_CHAR) // RAX = 17 (SYSCALL_READ_CHAR)
        : "cc", "memory"
    );
    return (char)ret;
}

static inline void nano_clear_screen(void) {
    __asm__ volatile (
        "int $0x80"
        : 
        : "a" ((uint64_t)SYSCALL_CLEAR_SCREEN) // SYSCALL_CLEAR_SCREEN
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
    register uint64_t rax_val __asm__("rax") = 20; // SYSCALL_EXIT
    register uint64_t rdi_val __asm__("rdi") = (uint64_t)status;
    
    __asm__ volatile (
        "int $0x80"
        : 
        : "r"(rax_val), "r"(rdi_val)
        : "cc", "memory"
    );
    while(1);
}

static inline void nano_sleep(uint32_t milliseconds) {
    __asm__ volatile (
        "int $0x80"
        : // Nu avem variabile de ieșire
        : "a" ((uint64_t)SYSCALL_SLEEP), "D" ((uint64_t)milliseconds) // "a" forțează RAX = 4, "D" forțează RDI = milliseconds
        : "cc", "memory"
    );
}

static inline void sleep(uint32_t milliseconds) {
	return nano_sleep(milliseconds);
}

static inline char* nano_strcat(char* dest, const char* src) {
    char* ptr = dest + strlen(dest);
    while (*src != '\0') {
        *ptr++ = *src++;
    }
    *ptr = '\0';
    return dest;
}

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
        : "a" ((uint64_t)SYSCALL_HAS_CHAR) // SYSCALL_HAS_CHAR = 18
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

static inline int nano_mount(const char* fs_name) {
    int ret;
    __asm__ volatile (
        "movq $37, %%rax\n"       // Numărul syscall-ului (SYSCALL_MOUNT) - 64-bit
        "movq %1, %%rdi\n"        // Argumentul (pointerul fs_name) în reg RDI (64-bit)
        "int $0x80\n"            // Întreruperea pentru apelul către kernel
        "movl %%eax, %0\n"       // Salvăm rezultatul de 32-bit în variabila 'ret'
        : "=r" (ret)
        : "r" (fs_name)
        : "rax", "rdi", "memory"
    );
    return ret;
}

static inline void nano_fdisk(void) {
    __asm__ volatile (
        "int $0x80"
        :
        : "a" ((uint64_t)38) // 38 este SYSCALL_FDISK
        : "cc", "memory"
    );
}

static inline int nano_get_fs_stats(const char* path, DiskStats* stats) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(39), "D"(path), "S"(stats) // 39 este SYSCALL_DISK_STATS, RDI = path, RSI = stats
        : "memory"
    );
    return (int)ret;
}

static inline int nano_get_video_info(VideoModeInfo* info) {
    uint64_t ret;
    __asm__ volatile (
        "mov $40, %%rax\n"          // SYSCALL_VIDEO_INFO = 40
        "mov %1, %%rdi\n"          // Pointerul către structura din user-space
        "int $0x80\n"              // Sau instrucțiunea ta de întrerupere pentru syscall
        "mov %%rax, %0\n"
        : "=r"(ret)
        : "r"(info)
        : "rax", "rdi", "memory"
    );
    return (int)ret;
}

static inline void nano_swap_buffers() {
    __asm__ volatile (
        "mov $41, %%rax\n"
        "int $0x80\n"
        ::: "rax", "memory"
    );
}

static inline void nano_draw_full_screen(const void* screen_buffer) {
    __asm__ volatile (
        "mov $42, %%rax\n\t"       // Numărul syscall-ului
        "mov %0, %%rdx\n\t"       // Adresa bufferului din user-space trimisă în rdx
        "int $0x80\n\t"           // Declanșăm întreruperea software
        :
        : "r" (screen_buffer)     // Input: pointerul către buffer
        : "rax", "rdx", "memory"  // Registre modificate și barieră de memorie
    );
}

static inline int nano_get_file_name_at(int index, char* buf, int max_len) {
    (void)max_len;
    long success = 0;
    
    register long rdi_val __asm__("rdi") = (long)index;
    register long rsi_val __asm__("rsi") = (long)buf;
    register long rax_val __asm__("rax") = 43; // ID-ul exact al syscall-ului în kernel

    __asm__ volatile (
        "int $0x80\n\t"
        : "=r" (rax_val)
        : "r" (rax_val), "r" (rdi_val), "r" (rsi_val)
        : "memory"
    );

    success = rax_val;
    return (int)success;
}

static inline int fork(void) {
    long success = 0;
    
    register long rax_val __asm__("rax") = 44; // SYSCALL_FORK

    __asm__ volatile (
        "int $0x80\n\t"
        : "=r" (rax_val)
        : "r" (rax_val)
        : "memory"
    );

    success = rax_val;
    return (int)success;
}

static inline uint32_t nano_create_thread(void (*entry_point)(void*), void* arg) {
    // Folosești constanta din header (dacă ai inclus syscall.h în user-space) 
    // SAU pui direct 45 ca număr:
    register long rax_val __asm__("rax") = 45; 
    register long rdi_val __asm__("rdi") = (long)entry_point;
    register long rsi_val __asm__("rsi") = (long)arg;

    __asm__ volatile (
        "int $0x80\n\t"
        : "+r" (rax_val)
        : "r" (rdi_val), "r" (rsi_val)
        : "memory"
    );

    return (uint32_t)rax_val; 
}

static inline uint32_t nano_thread_create(void* entry_point, void* arg) {
    uint64_t tid;
    __asm__ volatile (
        "mov $24, %%rax\n"     // IMPORTANT: Înlocuiește 24 cu numărul tău real pentru SYSCALL_CREATE_THREAD
        "mov %1, %%rdi\n"
        "mov %2, %%rsi\n"
        "int $0x80\n"
        "mov %%rax, %0\n"
        : "=r" (tid)
        : "r" ((uint64_t)entry_point), "r" ((uint64_t)arg)
        : "rax", "rdi", "rsi"
    );
    return (uint32_t)tid;
}


static inline void nano_thread_exit(int exit_code) {
    __asm__ volatile (
        "mov $46, %%rax\n"     // 25 = înlocuiește cu numărul tău pentru SYSCALL_THREAD_EXIT
        "mov %0, %%rdi\n"
        "int $0x80\n"
        :: "r" ((uint64_t)exit_code) : "rax", "rdi"
    );
    while(1); // Nu ar trebui să se mai întoarcă aici niciodată
}

static inline void nano_yield() {
    register long rax_val __asm__("rax") = 47; // sau ce număr are SYSCALL_YIELD
    __asm__ volatile ("int $0x80" : : "r" (rax_val) : "memory");
}

//Spinlock Atomic (Mutex în User-Space)
//Procesoarele x86-64 au instrucțiuni speciale pe care nu le poate întrerupe nici măcar schedulerul tău. Cea mai utilă este xchg (exchange), care schimbă valoarea dintr-un registru cu una din memorie într-un singur ciclu de ceas atomic.
//Spinlock-ul de mai sus funcționează perfect, dar are un dezavantaj pe un singur nucleu de procesor: dacă Thread-ul 2 vrea lacătul și Thread-ul 1 îl are, Thread-ul 2 se va învârti de mii de ori în while(1), consumând milisecundele prețioase de la scheduler degeaba, până când îi expiră timpul.
//Un Mutex adevărat își dă seama că nu are rost să aștepte arzând CPU și cedează de bună voie controlul înapoi scheduler-ului!
static inline void nano_mutex_spinlock(nano_mutex_t* lock) {
    int expected = 1;
    // Buclează până când reușește să schimbe 0 (liber) în 1 (blocat)
    while (1) {
        __asm__ volatile (
            "xchg %0, %1"
            : "+r" (expected), "+m" (*lock)
            :
            : "memory"
        );
        // Dacă valoarea anterioară era 0, înseamnă că am pus noi 1 și am luat lacătul!
        if (expected == 0) {
            break;
        }
        // Dacă era deja 1, altcineva are lacătul, așa că se învârte în buclă (Spin)
        expected = 1; 
    }
}


// 1. Funcția de BLOCARE (Yielding Lock) SYSCALL_YIELD
static inline void nano_mutex_lock(nano_mutex_t* lock) {
    int expected = 1;
    while (1) {
        // Încercăm să luăm lacătul atomic
        __asm__ volatile (
            "xchg %0, %1"
            : "+r" (expected), "+m" (*lock)
            :
            : "memory"
        );
        
        // Dacă era 0, am pus noi 1 și am câștigat lacătul! Ieșim din buclă.
        if (expected == 0) {
            break; 
        }
        
        // Dacă era deja 1 (altcineva îl are), nu ardem procesorul degeaba!
        // Cedăm execuția înapoi către scheduler până la următorul tic.
        nano_yield(); 
        
        // Resetăm variabila pentru următoarea încercare
        expected = 1;
    }
}


// Funcția de DEBLOCARE (Unlock)
static inline void nano_mutex_unlock(nano_mutex_t* lock) {
    // Punem înapoi 0 (liber). Instrucțiunea de scriere pe x86 este natural atomică.
    __asm__ volatile (
        "movl $0, %0"
        : "+m" (*lock)
        :
        : "memory"
    );
}

static inline int nano_thread_join(uint32_t tid) {
    uint64_t ret;
    
    while (1) {
        __asm__ volatile (
            "mov $50, %%rax\n"     // Folosește numărul corect de syscall pentru JOIN
            "mov %1, %%rdi\n"
            "int $0x80\n"
            "mov %%rax, %0\n"
            : "=r" (ret)
            : "r" ((uint64_t)tid)
            : "rax", "rdi"
        );
        
        // Dacă am primit -2, înseamnă că syscall-ul tocmai ne-a trecut în SLEEPING.
        // Imediat cum se termină `int 0x80`, noi (firul curent) suntem adormiți de kernel.
        // Așa că bucla asta NU VA CONSUMA CPU. 
        // Când copilul moare, ne trezește (ne face READY), iar noi dăm automat un nou `int 0x80`!
        if ((int64_t)ret != -2) {
            break; // A returnat un cod real sau -1 (eroare), putem ieși.
        }
    }
    
    return (int)ret;
}



static inline uint32_t nano_getuid(void) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (SYSCALL_GETUID) // Folosește macro-ul din syscall.h
        : "memory"
    );
    return (uint32_t)ret;
}

static inline int nano_setuid(uint32_t uid) {
    register long rax_val __asm__("rax") = 49; // Numărul syscall-ului setuid
    register long rdi_val __asm__("rdi") = (long)uid;
    __asm__ volatile (
        "int $0x80\n\t"
        : "+r" (rax_val)
        : "r" (rdi_val)
        : "memory"
    );
    return (int)rax_val;
}

static inline int nano_net_send(const void* data, uint32_t length) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a" (ret)
        : "a" (SYSCALL_NET_SEND), "D" (data), "S" (length)
        : "memory"
    );
    return ret;
}

static inline int nano_ping(uint32_t dest_ip) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a" (ret)
        : "a" (SYSCALL_NET_SEND), "D" (dest_ip)
        : "memory"
    );
    return ret;
}

// Wrapper pentru recepție în user-space
static inline int nano_net_recv(void* buffer, uint32_t max_len) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a" (ret)
        : "a" (SYSCALL_NET_RECV), "D" (buffer), "S" (max_len)
        : "memory"
    );
    return ret;
}


// --- Socket Wrappers pentru User-Space (Ring 3) ---

static inline int socket(int domain, int type, int protocol) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a" (ret)
        : "a" (SYSCALL_SOCKET), "D" (domain), "S" (type), "d" (protocol)
        : "memory"
    );
    return ret;
}

static inline int bind(int fd, uint16_t port) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a" (ret)
        : "a" (SYSCALL_BIND), "D" (fd), "S" (port)
        : "memory"
    );
    return ret;
}

static inline int listen(int fd, int backlog) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a" (ret)
        : "a" (SYSCALL_LISTEN), "D" (fd), "S" (backlog)
        : "memory"
    );
    return ret;
}

static inline int accept(int server_fd) {
    int ret;
    while (1) {
        __asm__ volatile(
            "int $0x80"
            : "=a" (ret)
            : "a" (SYSCALL_ACCEPT), "D" (server_fd)
            : "memory"
        );
        // Dacă kernelul a returnat -2, înseamnă că ne-a adormit în PROC_SLEEPING.
        // Când un client se conectează, kernelul ne trezește și repetăm apelul automat.
        if (ret != -2) {
            break;
        }
    }
    return ret;
}

static inline int send_socket(int fd, const void* buf, uint32_t len) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a" (ret)
        : "a" (SYSCALL_SEND), "D" (fd), "S" (buf), "d" (len)
        : "memory"
    );
    return ret;
}

static inline int recv_socket(int fd, void* buf, uint32_t max_len) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a" (ret)
        : "a" (SYSCALL_RECV), "D" (fd), "S" (buf), "d" (max_len)
        : "memory"
    );
    return ret;
}

static inline int close_socket(int fd) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a" (ret)
        : "a" (SYSCALL_CLOSE), "D" (fd)
        : "memory"
    );
    return ret;
}

// --- TTY Wrappers pentru User-Space ---

static inline int nano_tty_inject(int tty_id, char c) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a" (ret)
        : "a" (SYSCALL_TTY_INJECT), "D" (tty_id), "S" (c)
        : "memory"
    );
    return ret;
}

static inline int nano_set_tty(int tty_id) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a" (ret)
        : "a" (SYSCALL_SET_TTY), "D" (tty_id)
        : "memory"
    );
    return ret;
}

static inline int nano_read_tty_output(int tty_id, char* buf, uint32_t max_len) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a" (ret)
        : "a" (SYSCALL_READ_TTY_OUTPUT), "D" (tty_id), "S" (buf), "d" (max_len)
        : "memory"
    );
    return ret;
}

static inline int nano_set_stdout_file(const char* filepath) {
    int result = 0;
    __asm__ volatile (
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_SET_STDOUT), "D"(filepath) // RDI conține calea fișierului
        : "memory"
    );
    return result;
}

static inline int nano_gfx_present(int x, int y, int w, int h, uint32_t* pixels) {
    register uint64_t syscall_num __asm__("rax") = SYSCALL_GFX_PRESENT;
    register uint64_t arg_x       __asm__("rdi") = (uint64_t)x;
    register uint64_t arg_y       __asm__("rsi") = (uint64_t)y;
    register uint64_t arg_w       __asm__("rdx") = (uint64_t)w;
    register uint64_t arg_h       __asm__("rcx") = (uint64_t)h;
    register uint64_t arg_ptr     __asm__("r8")  = (uint64_t)pixels;
    register uint64_t ret         __asm__("rax");

    __asm__ volatile (
        "int $0x80"
        : "=r"(ret)
        : "r"(syscall_num), "r"(arg_x), "r"(arg_y), "r"(arg_w), "r"(arg_h), "r"(arg_ptr)
        : "memory"
    );

    return (int)ret;
}

static inline void nano_set_desktop_mode(int mode) {
    __asm__ volatile (
        "int $0x80"
        : 
        : "a"((uint64_t)SYSCALL_DESKTOP_MODE), "D"((uint64_t)mode)
        : "cc", "memory"
    );
}

static inline int nano_get_tty_buffer(int tty_id, uint16_t* buffer) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"((uint64_t)SYSCALL_GET_TTY_BUFFER), "D"((uint64_t)tty_id), "S"((uint64_t)buffer)
        : "memory", "cc"
    );
    return (int)ret;
}

static inline int nano_get_mouse_state(MouseState* state) {
    uint64_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"((uint64_t)SYSCALL_GET_MOUSE_STATE), "D"((uint64_t)state)
        : "memory", "cc"
    );
    return (int)ret;
}

static inline int nano_get_key_event(key_event_t* event) {
    int result;
    
    // RAX primește ID-ul syscall-ului (SYSCALL_GET_KEY_EVENT)
    // RDI primește pointerul către structura 'event'
    // La întoarcere, RAX va conține rezultatul (1 sau 0)
    __asm__ volatile (
        "int $0x80"
        : "=a" (result)
        : "a" (SYSCALL_GET_KEY_EVENT), "D" ((uint64_t)event)
        : "memory"
    );
    
    return result;
}

static inline uint32_t nano_get_ticks(void) {
    uint32_t ticks;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ticks)
        : "a"(SYSCALL_GET_TICKS)
    );
    return ticks;
}

static inline int nano_get_username(uint32_t uid, char* buf, size_t max_len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (SYSCALL_GET_USERNAME), "D" ((uint64_t)uid), "S" ((uint64_t)buf), "d" ((uint64_t)max_len)
        : "memory"
    );
    return ret;
}

static inline int nano_get_user_info(const char* username, uint32_t* out_uid, char* out_home) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a" (SYSCALL_GET_USER_INFO), "D" ((uint64_t)username), "S" ((uint64_t)out_uid), "d" ((uint64_t)out_home)
        : "memory"
    );
    return ret;
}

static inline int nano_chmod(const char* path, uint16_t mode) {
    uint64_t res;
    __asm__ volatile (
        "mov $25, %%rax\n"     // IMPORTANT: Înlocuiește 25 cu numărul tău real pentru SYSCALL_CHMOD
        "mov %1, %%rdi\n"
        "mov %2, %%rsi\n"
        "int $0x80\n"
        "mov %%rax, %0\n"
        : "=r" (res)
        : "r" ((uint64_t)path), "r" ((uint64_t)mode)
        : "rax", "rdi", "rsi"
    );
    return (int)res;
}

static inline int nano_chown(const char* path, uint32_t uid, uint32_t gid) {
    uint64_t res;
    __asm__ volatile (
        "mov $26, %%rax\n"     // IMPORTANT: Înlocuiește 26 cu numărul tău real pentru SYSCALL_CHOWN
        "mov %1, %%rdi\n"
        "mov %2, %%rsi\n"
        "mov %3, %%rdx\n"      // GID-ul se duce în rdx
        "int $0x80\n"
        "mov %%rax, %0\n"
        : "=r" (res)
        : "r" ((uint64_t)path), "r" ((uint64_t)uid), "r" ((uint64_t)gid)
        : "rax", "rdi", "rsi", "rdx"
    );
    return (int)res;
}

#endif

