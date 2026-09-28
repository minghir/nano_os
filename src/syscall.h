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

// Structura care se potrivește exact cu ordinea push-urilor din Assembly
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
} __attribute__((packed)) SyscallRegisters;

void syscall_handler(SyscallRegisters* regs);
#endif