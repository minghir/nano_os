#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>

// Codurile pentru Syscall-uri (System Call Numbers)

typedef enum {
    SYSCALL_PRINT = 1,
    SYSCALL_READLINE,
    SYSCALL_MALLOC,
    SYSCALL_SLEEP,
    SYSCALL_DATETIME,
    SYSCALL_EXEC,
    SYSCALL_SHUTDOWN,
    SYSCALL_LIST_FILES,
    SYSCALL_CD,
    SYSCALL_CREATE_FILE,
    SYSCALL_WRITE_FILE,
    SYSCALL_FORMAT,
    SYSCALL_DELETE_FILE,
    SYSCALL_READ_FILE,
    SYSCALL_MKDIR,
    SYSCALL_PRINT_INT,
    SYSCALL_READ_CHAR,
    SYSCALL_CLEAR_SCREEN,
    SYSCALL_PWD,
    SYSCALL_EXIT,
    SYSCALL_PS,
    SYSCALL_WAIT,
    SYSCALL_KILL,
    SYSCALL_MEMINFO,
    SYSCALL_GETCWD,
    SYSCALL_NEWLINE,
    SYSCALL_SYSLOG,
    SYSCALL_GETLOG,
    SYSCALL_HAS_CHAR,
    SYSCALL_FREE,
    SYSCALL_PRINT_FLOAT,
    SYSCALL_REBOOT,
    SYSCALL_GETPID,
    SYSCALL_BEEP,
    SYSCALL_SET_CURSOR_SHAPE,
    SYSCALL_PLAY_AUDIO,
    SYSCALL_MOUNT,
    SYSCALL_FDISK,
    SYSCALL_DISK_STATS,
    SYSCALL_VIDEO_INFO,
    SYSCALL_SWAP_VIDEO_BUFFERS,
    SYSCALL_DRAW_FRAME,
    SYSCALL_GET_DIR_ENTRIES,
    SYSCALL_FORK,
    SYSCALL_CREATE_THREAD,
    SYSCALL_THREAD_EXIT,
    SYSCALL_YIELD,
    SYSCALL_GETUID,
    SYSCALL_SETUID,
    SYSCALL_THREAD_JOIN,
	SYSCALL_NET_SEND,
	SYSCALL_NET_RECV,
	// --- Noi syscall-uri pentru rețea / sockets în User-Space ---
    SYSCALL_SOCKET,
    SYSCALL_BIND,
    SYSCALL_LISTEN,
    SYSCALL_ACCEPT,
    SYSCALL_SEND,
    SYSCALL_RECV,
    SYSCALL_CLOSE,
	SYSCALL_TTY_INJECT,
	SYSCALL_SET_TTY,
	SYSCALL_READ_TTY_OUTPUT,
	SYSCALL_SET_STDOUT,
	SYSCALL_GFX_PRESENT,
	SYSCALL_DESKTOP_MODE,
	SYSCALL_GET_TTY_BUFFER,
	SYSCALL_GET_MOUSE_STATE,
	SYSCALL_GET_KEY_EVENT,
	SYSCALL_GET_TICKS,
} SyscallNumber;

// Structura care se potrivește exact cu ordinea push-urilor din Assembly
typedef struct {
    uint8_t xmm0[16];
    uint64_t padding;
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
