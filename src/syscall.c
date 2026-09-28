#include <stdint.h>
#include <stddef.h>  // Pentru size_t

#include "memory.h"  // Pentru funcția malloc()
#include "syscall.h"
#include "io.h"
#include "timer.h"
//#include "shell.h"
#include "fs.h"
#include "string.h"

extern char env_path[];

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
        case SYSCALL_EXEC: {
            char* command = (char*)regs->rdi;
            uint8_t* prog_memory = (uint8_t*)0x900000; // Aplicațiile rulează la 9 MB
            int file_found = 0;

            char paths_to_try[6][64];
            int try_count = 0;

            // Resetăm bufferele
            for (int i = 0; i < 6; i++) {
                for (int j = 0; j < 64; j++) {
                    paths_to_try[i][j] = '\0';
                }
            }

            // Dacă s-a dat o cale absolută sau relativă directă
            if (command[0] == '/' || command[0] == '.') {
                string_copy(paths_to_try[try_count++], command);
            } else {
                // Căutăm folosind variabila PATH din kernel (env_path)
                char path_copy[128];
                string_copy(path_copy, env_path); // env_path definit în shell.c vechi sau kernel
                
                int start_idx = 0;
                int len = string_length(path_copy);
                
                for (int i = 0; i <= len; i++) {
                    if (path_copy[i] == ';' || path_copy[i] == '\0') {
                        path_copy[i] = '\0'; 
                        char* current_dir = &path_copy[start_idx];
                        start_idx = i + 1;
                        
                        if (string_length(current_dir) == 0) continue;

                        string_copy(paths_to_try[try_count], current_dir);
                        int dirlen = string_length(paths_to_try[try_count]);
                        if (dirlen > 0 && paths_to_try[try_count][dirlen-1] != '/') {
                            string_concat(paths_to_try[try_count], "/");
                        }
                        string_concat(paths_to_try[try_count], command);
                        try_count++;
                        
                        if (try_count >= 5) break;
                    }
                }
            }

            // Testăm pe disc rând pe rând căile generate
            for (int i = 0; i < try_count; i++) {
                int bytes_read = fs_read_file(paths_to_try[i], prog_memory, 16384);
                if (bytes_read > 0) {
                    file_found = 1;
                    void (*program_start)(void) = (void (*)(void))prog_memory;
                    program_start(); // Execută programul, care va da 'ret' la final
                    break;
                }
            }

            regs->rax = file_found ? 1 : 0; // Returnează 1 dacă s-a executat, 0 altfel
            break;
        }
        case SYSCALL_SHUTDOWN: {
            print("Kernel: Shutdown cerut din User Space...\n");
            // Trimitem semnalul de oprire ACPI prin porturile standard QEMU/Bochs
            __asm__ volatile ("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));
            __asm__ volatile ("outw %0, %1" : : "a"((uint16_t)0x0160), "Nd"((uint16_t)0xB004));
            break;
        }
        case SYSCALL_LIST_FILES: {
            fs_list_files();   // Folosește directorul curent din kernel
            break;
        }
        case SYSCALL_CD: {
            const char* path = (const char*)regs->rdi;
            int ok = fs_cd(path);
            regs->rax = ok;   // 1 = succes, 0 = eroare
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