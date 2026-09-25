#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>

// Structura care se potrivește exact cu ordinea push-urilor din Assembly
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
} __attribute__((packed)) SyscallRegisters;

void syscall_handler(SyscallRegisters* regs);
#endif