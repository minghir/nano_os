#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>



// Codurile pentru Syscall-uri (System Call Numbers)
#define SYSCALL_PRINT       1  // Afișează un șir de caractere (string) pe ecran
#define SYSCALL_READLINE    2  // Citește o linie de text de la tastatură într-un buffer
#define SYSCALL_MALLOC      3  // Alocă memorie din heap-ul user-space
#define SYSCALL_SLEEP       4  // Pune procesul curent în pauză (sleep) pentru un număr de milisecunde
#define SYSCALL_DATETIME    5  // Returnează data și ora curentă (sincronizată cu offsetul de timezone)
#define SYSCALL_EXEC        6  // Încarcă și execută un program binar nou din sistemul de fișiere
#define SYSCALL_SHUTDOWN    7  // Oprește sistemul de operare / închide emulatorul
#define SYSCALL_LIST_FILES  8  // Listează fișierele și directoarele din directorul curent
#define SYSCALL_CD          9  // Schimbă directorul curent (change directory)
#define SYSCALL_CREATE_FILE 10 // Creează un fișier nou gol pe disc
#define SYSCALL_WRITE_FILE  11 // Scrie date într-un fișier existent
#define SYSCALL_FORMAT      12 // Formatează discul virtual (resetează sistemul de fișiere NAN2)
#define SYSCALL_DELETE_FILE 13 // Șterge un fișier de pe disc
#define SYSCALL_READ_FILE   14 // Citește conținutul unui fișier într-un buffer
#define SYSCALL_MKDIR       15 // Creează un director nou (make directory)
#define SYSCALL_PRINT_INT   16 // Afișază valoarea numerică dintr-un registru (ex: rax) pe ecran
#define SYSCALL_READ_CHAR   17 // Citește un singur caracter de la tastatură (non-blocking / blocant în coadă)
#define SYSCALL_CLEAR_SCREEN 18 // Curăță ecranul VGA și resetează cursorul sus
#define SYSCALL_PWD         19 // Returnează calea completă a directorului curent (Print Working Directory)
#define SYSCALL_EXIT        20 // Termină execuția procesului curent și eliberează resursele (Exit)
#define SYSCALL_PS          21 // Afișează lista proceselor active din sistem (Process Status)
#define SYSCALL_WAIT        22 // Așteaptă ca un proces copil să se termine (Wait)
#define SYSCALL_KILL        23 // Oprește forțat un proces pe baza PID-ului
#define SYSCALL_MEMINFO     24 // Afișază informații despre memoria RAM / Heap total și folosit
#define SYSCALL_GETCWD      25 // Copiază calea directorului curent într-un buffer din user-space
#define SYSCALL_NEWLINE     26 // Inserează un rând nou (newline) pe ecran cu tot cu scroll automat
#define SYSCALL_SYSLOG		27 // Scrie la syslog
#define SYSCALL_GETLOG		28 //citeste de la syslog
#define SYSCALL_HAS_CHAR	29 //
#define SYSCALL_FREE		30
#define SYSCALL_PRINT_FLOAT  31
#define SYSCALL_REBOOT		32
#define SYSCALL_GETPID		33
#define SYSCALL_BEEP		34
#define SYSCALL_SET_CURSOR_SHAPE 35
#define SYSCALL_PLAY_AUDIO       36
#define SYSCALL_MOUNT            37 // Syscall nou pentru montarea/schimbarea sistemului de fișiere
#define SYSCALL_FDISK            38 // Syscall nou pentru montarea/schimbarea sistemului de fișiere
#define SYSCALL_DISK_STATS       39
#define SYSCALL_VIDEO_INFO      40
#define SYSCALL_SWAP_VIDEO_BUFFERS 41
#define SYSCALL_DRAW_FRAME		42
#define SYSCALL_GET_DIR_ENTRIES	43
// Structura care se potrivește exact cu ordinea push-urilor din Assembly
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
} __attribute__((packed)) SyscallRegisters;

//header executabil NAS1

typedef struct {
    char magic[4];       // "NAS1"
    uint32_t entry_offset;
} __attribute__((packed)) NanoHeader;

typedef struct {
    uint64_t heap_total;
    uint64_t heap_used;
    uint64_t heap_free;
    uint64_t physical_total; // opțional, dacă vrei să afișezi și RAM-ul total
    uint64_t physical_free;
} MemInfo;

typedef struct {
    uint32_t total_sectors;
    uint32_t free_sectors;
    uint32_t sector_size; // De obicei 512
} __attribute__((packed)) DiskStats;

void syscall_handler(SyscallRegisters* regs);
#endif
