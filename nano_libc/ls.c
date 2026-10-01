#include "nano_libc.h"
#include "../src/syscall.h" // Sau de unde incluzi codurile de syscall în user-space

void sys_exit(int status) {
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

int main(int argc, char* argv[]) {
    const char* target_dir = ".";
    if (argc > 1) {
        target_dir = argv[1];
    }

    nano_ls(target_dir);
    
    // Încheiem execuția prin sistemul oficial de syscall-uri
    sys_exit(0);
    return 0;
}