#include <stdint.h>
#include <stddef.h>  // Pentru size_t

#include "memory.h"  // Pentru funcția malloc()
#include "syscall.h"
#include "io.h"
#include "timer.h"
#include "shell.h"

void syscall_handler(SyscallRegisters* regs) {
    // 1. RE-ACTIVĂM ÎNTRERUPERILE HARDWARE!
    __asm__ volatile ("sti");

    // 2. Rutăm apelul folosind un switch bazat pe macrourile definite
    // 2. Rutăm apelul tăind "gunoiul" din partea superioară a lui RAX
    switch ((uint32_t)regs->rax) {
        
        case SYSCALL_PRINT: {
            char* str = (char*)regs->rdi;
            print(str);
            break;
        }
        
        case SYSCALL_READLINE: {
            char* buffer = (char*)regs->rdi;
            uint32_t max_length = regs->rsi;
            keyboard_read_line(buffer, max_length); 
            break;
        }
        
        case SYSCALL_MALLOC: {
            size_t size = (size_t)regs->rdi;
            void* ptr = malloc(size);
            regs->rax = (uint64_t)ptr; // Returnăm adresa pointerului
            break;
        }
        
        case SYSCALL_SLEEP: {
            uint32_t milliseconds = (uint32_t)regs->rdi;
            print("Sleep cerut pt ms: "); 
            print_number(milliseconds);
            print("\n");
            sleep_ms(milliseconds); 
            break;
        }
        
        case SYSCALL_DATETIME: {
            DateTime* user_dt = (DateTime*)regs->rdi;
            DateTime kernel_dt = get_current_time();
            *user_dt = kernel_dt; // Copiem direct rezultatul
            break;
        }
        
        default: {
            // Un mic mecanism de protecție dacă programul cere un syscall inexistent
            print("Kernel Warning: Syscall necunoscut apelat: ");
            print_number(regs->rax);
            print("\n");
            break;
        }
    }
}