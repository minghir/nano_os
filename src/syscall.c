#include <stdint.h>
#include <stddef.h>  // Pentru size_t

#include "memory.h"  // Pentru funcția malloc()
#include "syscall.h"
#include "io.h"


void syscall_handler(SyscallRegisters* regs) {
    // 1. RE-ACTIVĂM ÎNTRERUPERILE HARDWARE!
    // Fără asta, tastatura (IRQ 1) nu ne poate trimite taste cât timp suntem în Syscall.
    __asm__ volatile ("sti");

    // Verificăm ce comandă cere programul în RAX
    if (regs->rax == 1) {
        // RAX = 1 înseamnă print string
        char* str = (char*)regs->rdi; // RDI conține adresa textului trimis de program
        print(str);
    }else if (regs->rax == 2) {
        // RAX = 2: Read string de la tastatură
        char* buffer = (char*)regs->rdi;
        uint32_t max_length = regs->rsi;
        
        // Aici apelăm funcția kernel-ului care așteaptă un rând de text.
        // Presupun că ai o funcție asemănătoare folosită de shell.
        keyboard_read_line(buffer, max_length); 
    }else if (regs->rax == 3) {
        // RAX = 3: Alocare memorie (malloc pentru user-space)
        size_t size = (size_t)regs->rdi;
        
        // Apelăm funcția ta de kernel malloc
        void* ptr = malloc(size);
        
        // Returnăm adresa pointerului în RAX
        regs->rax = (uint64_t)ptr;
    }
}