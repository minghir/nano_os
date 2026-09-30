#include <stdint.h>
#include <stddef.h>  // Pentru size_t

#include "memory.h"  // Pentru funcția malloc()
#include "syscall.h"
#include "io.h"
#include "timer.h"
#include "fs.h"
#include "string.h"
#include "process.h"

extern char env_path[];


uint64_t get_program_load_address(const char* path) {
    // Verificăm după nume sau cale exactă, la fel ca în Makefile
    
    // Programe din /sbin/
    if (string_contains(path, "shell"))   return 0x800000;
    if (string_contains(path, "time"))    return 0x810000;
    if (string_contains(path, "date"))    return 0x820000;
    if (string_contains(path, "shutdown"))return 0x830000;
    if (string_contains(path, "ls"))      return 0x840000;
    if (string_contains(path, "format"))  return 0x850000;
    if (string_contains(path, "rm"))      return 0x860000;
    if (string_contains(path, "cat"))     return 0x870000;
    if (string_contains(path, "mkdir"))   return 0x880000;
    if (string_contains(path, "touch"))   return 0x890000;
    if (string_contains(path, "pwd"))     return 0x8A0000;
    if (string_contains(path, "ps"))      return 0x8B0000;
	if (string_contains(path, "kill"))    return 0x8C0000;

    // Programe din /bin/
    if (string_contains(path, "mandel"))  return 0x900000;
    if (string_contains(path, "asm"))     return 0x910000;
    if (string_contains(path, "vi"))      return 0x920000;

    // Programe din /tests/
    if (string_contains(path, "chr"))         return 0xA00000;
    if (string_contains(path, "argt"))        return 0xA10000;
    if (string_contains(path, "test_malloc")) return 0xA20000;
    if (string_contains(path, "test_sleep"))  return 0xA30000;

    // Adresă implicită pentru orice alt program neprevăzut
    return 0xB00000;
}


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
/*
		case SYSCALL_EXEC: {
            char* command = (char*)regs->rdi;
            uint8_t* prog_memory = (uint8_t*)0x900000;
            int file_found = 0;

            // 1. Copiem comanda curățând orice potențial \n sau \r de la capăt
            char cmd_copy[128];
            int c_idx = 0;
            while (command[c_idx] != '\0' && command[c_idx] != '\n' && command[c_idx] != '\r' && c_idx < 127) {
                cmd_copy[c_idx] = command[c_idx];
                c_idx++;
            }
            cmd_copy[c_idx] = '\0';

            // 2. Tokenizăm (folosind indecși siguri)
            char* argv[16];
            int argc = 0;
            int p = 0;

            while (cmd_copy[p] != '\0' && argc < 15) {
                // Sărim peste spații
                while (cmd_copy[p] == ' ' || cmd_copy[p] == '\t') p++;
                if (cmd_copy[p] == '\0') break;

                argv[argc++] = &cmd_copy[p]; // Salvăm începutul argumentului

                // Mergem până la următorul spațiu
                while (cmd_copy[p] != '\0' && cmd_copy[p] != ' ' && cmd_copy[p] != '\t') p++;
                
                if (cmd_copy[p] != '\0') {
                    cmd_copy[p] = '\0'; // Terminăm string-ul curent izolat
                    p++;
                }
            }
            argv[argc] = NULL;

            if (argc == 0) {
                regs->rax = 0;
                break;
            }

            // argv[0] este acum garantat să fie curat, de ex. "argt"
            char* prog_name = argv[0];
            char paths_to_try[6][64];
            int try_count = 0;

            // Resetăm bufferele
            for (int i = 0; i < 6; i++) {
                for (int j = 0; j < 64; j++) {
                    paths_to_try[i][j] = '\0';
                }
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

            // 3. Testăm căile și executăm
            for (int i = 0; i < try_count; i++) {
                int bytes_read = fs_read_file(paths_to_try[i], prog_memory, 32768);
                if (bytes_read > (int)sizeof(NanoHeader)) {
                    NanoHeader* hdr = (NanoHeader*)prog_memory;
                    
                    if (hdr->magic[0] == 'N' && hdr->magic[1] == 'A' && 
                        hdr->magic[2] == 'S' && hdr->magic[3] == '1') {
                        
                        file_found = 1;

                        // SOLUȚIA ANTI-CRASH: Plasăm structurile de argumente la distanță sigură (0x980000)!
                        // Astfel nu vor mai fi șterse de secțiunea .bss a programului tău!
                        uintptr_t arg_dest_area = 0x980000;
                        uint64_t* user_argv = (uint64_t*)arg_dest_area;
                        char* string_pool = (char*)(user_argv + argc + 1);

                        char* current_pool_ptr = string_pool;
                        for (int a = 0; a < argc; a++) {
                            char* src_arg = argv[a];
                            char* dest_arg = current_pool_ptr;
                            while (*src_arg != '\0') {
                                *current_pool_ptr++ = *src_arg++;
                            }
                            *current_pool_ptr++ = '\0';
                            user_argv[a] = (uint64_t)dest_arg;
                        }
                        user_argv[argc] = 0; 

                        void (*program_start)(int, char**) = (void (*)(int, char**))(prog_memory + hdr->entry_offset);
                        program_start(argc, (char**)user_argv);
                        break;
                    }
                }
            }

            regs->rax = file_found ? 1 : 0;
            break;
        }
*/		
		case SYSCALL_EXEC: {
            char* command = (char*)regs->rdi;
            uint8_t* prog_memory = (uint8_t*)0x900000;
            int file_found = 0;

            // 1. Copiem comanda curățând orice potențial \n sau \r de la capăt
            char cmd_copy[128];
            int c_idx = 0;
            while (command[c_idx] != '\0' && command[c_idx] != '\n' && command[c_idx] != '\r' && c_idx < 127) {
                cmd_copy[c_idx] = command[c_idx];
                c_idx++;
            }
            cmd_copy[c_idx] = '\0';

            // 2. Tokenizăm (folosind indecși siguri)
            char* argv[16];
            int argc = 0;
            int p = 0;

            while (cmd_copy[p] != '\0' && argc < 15) {
                // Sărim peste spații
                while (cmd_copy[p] == ' ' || cmd_copy[p] == '\t') p++;
                if (cmd_copy[p] == '\0') break;

                argv[argc++] = &cmd_copy[p]; // Salvăm începutul argumentului

                // Mergem până la următorul spațiu
                while (cmd_copy[p] != '\0' && cmd_copy[p] != ' ' && cmd_copy[p] != '\t') p++;
                
                if (cmd_copy[p] != '\0') {
                    cmd_copy[p] = '\0'; // Terminăm string-ul curent izolat
                    p++;
                }
            }
            argv[argc] = NULL;

            if (argc == 0) {
                regs->rax = 0;
                break;
            }

            // argv[0] este acum garantat să fie curat, de ex. "argt"
            char* prog_name = argv[0];
            char paths_to_try[6][64];
            int try_count = 0;

            // Resetăm bufferele
            for (int i = 0; i < 6; i++) {
                for (int j = 0; j < 64; j++) {
                    paths_to_try[i][j] = '\0';
                }
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

            // 3. Testăm căile și executăm
            for (int i = 0; i < try_count; i++) {
				
				uint8_t* prog_memory = (uint8_t*)get_program_load_address(paths_to_try[i]);
				
                int bytes_read = fs_read_file(paths_to_try[i], prog_memory, 32768);
                if (bytes_read > (int)sizeof(NanoHeader)) {
                    NanoHeader* hdr = (NanoHeader*)prog_memory;
                    
                    if (hdr->magic[0] == 'N' && hdr->magic[1] == 'A' && 
                        hdr->magic[2] == 'S' && hdr->magic[3] == '1') {
                        
                        file_found = 1;

                        // SOLUȚIA ANTI-CRASH: Plasăm structurile de argumente la distanță sigură (0x980000)!
                        // Astfel nu vor mai fi șterse de secțiunea .bss a programului tău!
                        uintptr_t arg_dest_area = 0x980000;
                        uint64_t* user_argv = (uint64_t*)arg_dest_area;
                        char* string_pool = (char*)(user_argv + argc + 1);

                        char* current_pool_ptr = string_pool;
                        for (int a = 0; a < argc; a++) {
                            char* src_arg = argv[a];
                            char* dest_arg = current_pool_ptr;
                            while (*src_arg != '\0') {
                                *current_pool_ptr++ = *src_arg++;
                            }
                            *current_pool_ptr++ = '\0';
                            user_argv[a] = (uint64_t)dest_arg;
                        }
                        user_argv[argc] = 0; 

                        //void (*program_start)(int, char**) = (void (*)(int, char**))(prog_memory + hdr->entry_offset);
						uint64_t entry_point = (uint64_t)(prog_memory + hdr->entry_offset);
						// 1. Creăm noul proces!
                        process_create(prog_name, entry_point, argc, (char**)user_argv);
						current_process->state = PROC_SLEEPING;
						//file_found = 1;
                        //program_start(argc, (char**)user_argv);
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
