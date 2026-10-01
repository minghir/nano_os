#include <stdint.h>
#include <stddef.h>  // Pentru size_t

#include "memory.h"  // Pentru funcția malloc()
#include "syscall.h"
#include "io.h"
#include "timer.h"
#include "fs.h"
#include "string.h"
#include "process.h"
#include "paging.h"

extern char env_path[];

void syscall_handler(SyscallRegisters* regs) {
    // 1. Salvăm numărul syscall-ului local IMEDIAT, înainte ca orice întrerupere să-l poată atinge!
    uint32_t syscall_num = (uint32_t)regs->rax;

    // 1. RE-ACTIVĂM ÎNTRERUPERILE HARDWARE!
    __asm__ volatile ("sti");

    // 2. Rutăm apelul tăind "gunoiul" din partea superioară a lui RAX
    //switch ((uint32_t)regs->rax) {
    switch (syscall_num) {
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
            //print("Sleep cerut pt ms: "); 
            //print_number(milliseconds);
            //print("\n");
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
            int file_found = 0;

            // 1. Curățare și Tokenizare comandă (codul tău original)
            char cmd_copy[128];
            int c_idx = 0;
            while (command[c_idx] != '\0' && command[c_idx] != '\n' && command[c_idx] != '\r' && c_idx < 127) {
                cmd_copy[c_idx] = command[c_idx];
                c_idx++;
            }
            cmd_copy[c_idx] = '\0';

            char* argv[16];
            int argc = 0;
            int p = 0;

            while (cmd_copy[p] != '\0' && argc < 15) {
                while (cmd_copy[p] == ' ' || cmd_copy[p] == '\t') p++;
                if (cmd_copy[p] == '\0') break;
                argv[argc++] = &cmd_copy[p];
                while (cmd_copy[p] != '\0' && cmd_copy[p] != ' ' && cmd_copy[p] != '\t') p++;
                if (cmd_copy[p] != '\0') {
                    cmd_copy[p] = '\0';
                    p++;
                }
            }
            argv[argc] = NULL;

            if (argc == 0) {
                regs->rax = 0;
                break;
            }

            char* prog_name = argv[0];
            char paths_to_try[6][64];
            int try_count = 0;

            for (int i = 0; i < 6; i++) {
                for (int j = 0; j < 64; j++) paths_to_try[i][j] = '\0';
            }

            if (prog_name[0] == '/' || prog_name[0] == '.') {
                string_copy(paths_to_try[try_count++], prog_name);
            } else {
                char path_copy[128];
                string_copy(path_copy, env_path); 
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
                        string_concat(paths_to_try[try_count], prog_name);
                        try_count++;
                        if (try_count >= 5) break;
                    }
                }
            }

            // 2. Încărcarea folosind Paginarea (FĂRĂ adrese hardcodate)
            for (int i = 0; i < try_count; i++) {
                
                // A. Alocăm 8 pagini FIZICE (32KB) pentru a citi programul de pe disc
                uint64_t phys_prog_mem = (uint64_t)alloc_page();
                for(int p = 1; p < 8; p++) alloc_page();

                // Kernel-ul citește fișierul direct în RAM-ul fizic brut
                int bytes_read = fs_read_file(paths_to_try[i], (uint8_t*)phys_prog_mem, 32768);
                
                if (bytes_read > (int)sizeof(NanoHeader)) {
                    NanoHeader* hdr = (NanoHeader*)phys_prog_mem;
                    
                    if (hdr->magic[0] == 'N' && hdr->magic[1] == 'A' && 
                        hdr->magic[2] == 'S' && hdr->magic[3] == '1') {
                        
                        file_found = 1;

                        // B. Creăm o HARTĂ VIRTUALĂ nouă (PML4) doar pentru acest proces
                        uint64_t* process_pml4 = create_process_pml4();

                        // C. Mapăm memoria fizică unde e programul la ADRESA VIRTUALĂ UNIVERSALĂ (0x800000)
                        for (uint64_t offset = 0; offset < 32768; offset += 4096) {
                            map_page(process_pml4, 0x800000 + offset, phys_prog_mem + offset, 
                                     PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
                        }

                        // Plasăm argumentele în zona sigură (0x980000)
                        uintptr_t arg_dest_area = 0x980000;
                        uint64_t* user_argv = (uint64_t*)arg_dest_area;
                        char* string_pool = (char*)(user_argv + argc + 1);
                        char* current_pool_ptr = string_pool;
                        
                        for (int a = 0; a < argc; a++) {
                            char* src_arg = argv[a];
                            char* dest_arg = current_pool_ptr;
                            while (*src_arg != '\0') *current_pool_ptr++ = *src_arg++;
                            *current_pool_ptr++ = '\0';
                            user_argv[a] = (uint64_t)dest_arg;
                        }
                        user_argv[argc] = 0; 

                        // D. Entry point-ul se raportează acum mereu la 0x800000
                        uint64_t entry_point = 0x800000 + hdr->entry_offset;
                        
                        // E. Creăm procesul și îi trimitem harta lui de memorie (PML4)
                        process_create(prog_name, entry_point, argc, (char**)user_argv, (uint64_t)process_pml4);
                        
                        //current_process->state = PROC_SLEEPING;
                        break;
                    }
                }
            }

            regs->rax = file_found ? 1 : 0;
            break;
        }
		case SYSCALL_EXIT: {
            int exit_code = (int)regs->rdi;
            
            if (current_process) {
                current_process->exit_code = exit_code;
                
                // 1. Căutăm părintele și îl trezim PRIMUL
                for (int i = 0; i < MAX_PROCESSES; i++) {
                    if (process_table[i].pid == current_process->ppid) {
                        process_table[i].state = PROC_READY; // Trezim Shell-ul
                        keyboard_flush();
                        break;
                    }
                }
                
                // 2. Acum eliberăm slotul copilului
                current_process->state = PROC_FREE;
            }
            
            // 3. Oprim execuția curentă și lăsăm timer-ul să mute pe shell
            while(1) {
                __asm__ volatile ("sti; hlt");
            }
            break;
        }
		case SYSCALL_WAIT: { // SYSCALL_WAIT
            int children_alive = 0;
            
            // Verificăm dacă procesul curent (Shell-ul) mai are copii în viață
            for (int i = 0; i < MAX_PROCESSES; i++) {
                if (process_table[i].state != PROC_FREE && process_table[i].ppid == current_process->pid) {
                    children_alive = 1;
                    break;
                }
            }

            if (!children_alive) {
                regs->rax = 0; // Nu are copii de așteptat
                break;
            }

            // Dacă are copii care încă rulează, punem Shell-ul la somn
            current_process->state = PROC_SLEEPING;
            
            // Pentru a ne asigura că Shell-ul se oprește IMEDIAT și nu se întoarce 
            // în user-space înainte ca timer-ul să facă switch-ul, blocăm execuția 
            // într-un hlt controlat, așteptând ca următorul tic de ceas să schimbe contextul.
            while (current_process->state == PROC_SLEEPING) {
                __asm__ volatile ("sti; hlt");
            }

            regs->rax = 1; // Un copil tocmai și-a dat exit și ne-a trezit!
            break;
        }
		case SYSCALL_KILL: {
			int target_pid = (int)regs->rdi;
			
			// PROTECȚIE CRITICĂ: Nimeni nu are voie să omoare procesul INIT (PID 1)!
			if (target_pid == 1) {
				// Returnăm eroare în RAX (de ex: -1 sau 0, depinde cum semnalezi eșecul)
				regs->rax = 0; 
				break;
			}
			
			int killed = 0;

			for (int i = 0; i < MAX_PROCESSES; i++) {
				// Nu lăsăm pe nimeni să omoare kernelul (PID 0) sau pe sine însuși accidental prin kill simplu
				if (process_table[i].pid == target_pid && target_pid != 0) {
					// Eliberăm stiva alocată
					if (process_table[i].stack_base) {
						free((void*)process_table[i].stack_base);
					}
					// Resetăm slotul
					process_table[i].state = PROC_FREE;
					process_table[i].pid = 0;
					killed = 1;
					break;
				}
			}
			regs->rax = killed ? 1 : 0;
			break;
		}
		case SYSCALL_PS: { // SYSCALL_PS
            // Definim o structură simplificată pe care o vom trimite user-space-ului
            typedef struct {
                uint32_t pid;
                uint32_t ppid;
                uint32_t state;
                char name[32];
            } ProcessInfo;

            ProcessInfo* user_buf = (ProcessInfo*)regs->rdi;
            uint32_t max_entries = (uint32_t)regs->rsi;
            uint32_t count = 0;

            // Parcurgem tabela de procese și o copiem în bufferul user-ului
            for (int i = 0; i < MAX_PROCESSES && count < max_entries; i++) {
                if (process_table[i].state != PROC_FREE) {
                    user_buf[count].pid = process_table[i].pid;
                    user_buf[count].ppid = process_table[i].ppid;
                    user_buf[count].state = process_table[i].state;
                    
                    // Copiem numele
                    int j = 0;
                    while (process_table[i].name[j] && j < 31) {
                        user_buf[count].name[j] = process_table[i].name[j];
                        j++;
                    }
                    user_buf[count].name[j] = '\0';
                    
                    count++;
                }
            }
            
            regs->rax = count; // Returnăm câte procese am găsit
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
            const char* target_path = (const char*)regs->rdi; 
            fs_list_files(target_path);
            break;
        }
        case SYSCALL_CD: {
            const char* path = (const char*)regs->rdi;
            int ok = fs_cd(path);
            regs->rax = ok;   // 1 = succes, 0 = eroare
            break;
        }
        case SYSCALL_CREATE_FILE: {
            const char* name = (const char*)regs->rdi;
            uint32_t size = (uint32_t)regs->rsi;
            regs->rax = fs_create_file(name, size); // Returnează 1 la succes, 0 la eșec
            break;
        }

        case SYSCALL_WRITE_FILE: {
            // Presupunem că transmitem: RDI = nume, RSI = pointer date, RDX = dimensiune
            const char* name = (const char*)regs->rdi;
            const uint8_t* data = (const uint8_t*)regs->rsi;
            uint32_t size = (uint32_t)regs->rdx;
            regs->rax = fs_write_file(name, data, size); // Returnează 1 la succes, 0 la eșec
            break;
        }
        case SYSCALL_FORMAT: {
            fs_format();
            break;
        }

        case SYSCALL_DELETE_FILE: {
            const char* name = (const char*)regs->rdi;
            regs->rax = fs_delete_file(name); // Returnează 1 la succes, 0 la eșec
            break;
        }
        case SYSCALL_READ_FILE: {
            const char* name = (const char*)regs->rdi;
            uint8_t* buffer = (uint8_t*)regs->rsi;
            uint32_t max_size = (uint32_t)regs->rdx;
            regs->rax = fs_read_file(name, buffer, max_size); // Returnează numărul de octeți citiți
            break;
        }
        case SYSCALL_MKDIR: {
            const char* name = (const char*)regs->rdi;
            regs->rax = fs_mkdir(name); // Returnează 1 la succes, 0 la eșec
            break;
        }
        case SYSCALL_PRINT_INT: {
            uint64_t val = regs->rdi;
            print_number(val);
            //newline();
            break;
        }
		case SYSCALL_READ_CHAR: {
            // Presupunând că ai o funcție în kernel care citește un caracter (blochează până când se apasă o tastă)
            char c = keyboard_read_char(); 
            regs->rax = (uint64_t)c; // Returnăm caracterul prin registrul RAX
            break;
        }
        case SYSCALL_CLEAR_SCREEN: {
            clear_screen(); // Funcția ta existentă care curăță ecranul VGA în kernel
            break;
        }
		case SYSCALL_PWD: {
            char* user_buffer = (char*)regs->rdi;
            uint32_t max_len = (uint32_t)regs->rsi;
            fs_get_current_path(user_buffer, max_len);
            break;
        }
		case SYSCALL_MEMINFO: {
			MemInfo* info = (MemInfo*)regs->rdi; // Primul argument e pointerul unde scriem datele
			if (info) {
				info->heap_total = get_heap_total();
				info->heap_used  = get_heap_used();
				info->heap_free  = get_heap_free();
				// Poți completa și date despre paginile fizice dacă ai funcțiile create în memory.c
				regs->rax = 1; // Succes
			} else {
				regs->rax = 0; // Eroare pointer
			}
			break;
		}
		case SYSCALL_GETCWD: {
			char* user_buf = (char*)regs->rdi;
			uint32_t max_len = (uint32_t)regs->rsi;
			
			if (user_buf && max_len > 0) {
				fs_get_current_path(user_buf, max_len);
				regs->rax = 1; // Succes
			} else {
				regs->rax = 0;
			}
			break;
		}
		case SYSCALL_NEWLINE: {
			newline();
			//regs->rax = 1;
			break;
		}
        default: {
            print("Kernel Warning: Syscall necunoscut apelat: ");
            print_number(regs->rax);
            print(" | Parametru (RDI): ");
            print_number(regs->rdi);
            print("\n");
            break;
        }
    }
}
