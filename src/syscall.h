#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>


// Codurile pentru Syscall-uri (System Call Numbers)
#define SYSCALL_PRINT     1
#define SYSCALL_READLINE  2
#define SYSCALL_MALLOC    3
#define SYSCALL_SLEEP     4
#define SYSCALL_DATETIME  5
#define SYSCALL_EXEC      6
#define SYSCALL_SHUTDOWN  7
#define SYSCALL_LIST_FILES 8
#define SYSCALL_CD 9
#define SYSCALL_CREATE_FILE 10
#define SYSCALL_WRITE_FILE  11
#define SYSCALL_FORMAT      12
#define SYSCALL_DELETE_FILE 13
#define SYSCALL_READ_FILE 14
#define SYSCALL_MKDIR 15
#define SYSCALL_PRINT_INT 16
#define SYSCALL_READ_CHAR 17
#define SYSCALL_CLEAR_SCREEN 18
#define SYSCALL_PWD 19
#define SYSCALL_EXIT 20
#define SYSCALL_PS 21
#define SYSCALL_WAIT 22
#define SYSCALL_KILL 23

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

void syscall_handler(SyscallRegisters* regs);
#endif
