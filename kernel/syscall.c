#include <stdint.h>
#include <stddef.h>  // Pentru size_t

#include "memory.h"  // Pentru funcția malloc()
#include "syscall.h"
#include "io.h"
#include "timer.h"
#include "fs/fs.h"
#include "string.h"
#include "process.h"
#include "paging.h"
#include "syslog.h"
#include "tty.h"
#include "sys.h"
#include "string.h"
#include "speaker.h"
#include "drivers/sound/audio.h"
#include "video.h"

extern char env_path[];
extern char kernel_log_buffer[KERNEL_LOG_SIZE];

extern int term_cols;
extern int term_rows;

void syscall_handler(SyscallRegisters* regs) {
	
	// 1. Dezactivăm întreruperile pentru a preveni schimbarea lui current_process la mijlocul syscall-ului
    __asm__ volatile("cli");
	
    // 1. Salvăm numărul syscall-ului local IMEDIAT, înainte ca orice întrerupere să-l poată atinge!
    uint32_t syscall_num = (uint32_t)regs->rax;

    // 2. Rutăm apelul tăind "gunoiul" din partea superioară a lui RAX
    //switch ((uint32_t)regs->rax) {
    switch (syscall_num) {
		case 0: { // SYSCALL 0 (Probabil sys_read, sys_yield sau getch)
            // Punem procesorul pe pauză până apare o întrerupere hardware (ex: timer sau tastatură)
            // Astfel tăiem spam-ul infinit și lăsăm CPU-ul să respire.
            __asm__ volatile ("sti; hlt");
            
            // Alternativ, dacă 0 e sys_read pentru tastatură, aici ai returna caracterul.
            regs->rax = 0; 
            break;
        }
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
		case SYSCALL_FREE: {
            void* ptr = (void*)regs->rdi;
            free(ptr); // Apelează funcția ta de eliberare din memory.h
            break;
        }
        
        case SYSCALL_SLEEP: {
            uint32_t milliseconds = (uint32_t)regs->rdi;
            //print("Sleep cerut pt ms: "); 
            //print_number(milliseconds);
            //print("\n");
            __asm__ volatile ("sti");
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

            // 1. Curățare și Tokenizare comandă
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
				
				// 1. Încercăm MAI ÎNTÂI în directorul curent al procesului care a cerut execuția!
                if (current_process && current_process->current_path[0] != '\0') {
                    string_copy(paths_to_try[try_count], current_process->current_path);
                    int dlen = string_length(paths_to_try[try_count]);
                    if (dlen > 0 && paths_to_try[try_count][dlen-1] != '/') {
                        string_concat(paths_to_try[try_count], "/");
                    }
                    string_concat(paths_to_try[try_count], prog_name);
                    try_count++;
                }
				
				// 2. Apoi parcurgem PATH-ul clasic din env_path
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

            // 2. VERIFICARE PREALABILĂ (Fără alocare de memorie fizică masivă)
            int valid_path_idx = -1;
            uint8_t temp_header_buf[512]; // Folosim doar 512 bytes temporari pe stivă
            
            for (int i = 0; i < try_count; i++) {
                int bytes_read = fs_read_file(paths_to_try[i], temp_header_buf, 512);
                
                if (bytes_read >= (int)sizeof(NanoHeader)) {
                    NanoHeader* temp_hdr = (NanoHeader*)temp_header_buf;
                    // Verificăm imediat dacă este un executabil valid Nano OS
                    if (temp_hdr->magic[0] == 'N' && temp_hdr->magic[1] == 'A' && 
                        temp_hdr->magic[2] == 'S' && temp_hdr->magic[3] == '1') {
                        valid_path_idx = i;
                        break; // Am găsit calea corectă!
                    }
                }
            }

            if (valid_path_idx == -1) {
                regs->rax = 0; // Fișierul nu există sau nu e executabil valid
                break;
            }

            // 3. ÎNCĂRCAREA EFECTIVĂ (Alocăm memorie doar pentru programul valid!)
            file_found = 1;
            
            // Creăm o HARTĂ VIRTUALĂ nouă (PML4) doar pentru acest proces
            uint64_t* process_pml4 = create_process_pml4();

            // A. Alocăm 8 pagini FIZICE disjuncte (fragmentate)
            uint64_t allocated_pages[MAX_PROG_PAGES];
            for(int p = 0; p < MAX_PROG_PAGES; p++) {
                allocated_pages[p] = (uint64_t)alloc_page();
                // Mapăm imediat pagina fizică (oriunde ar fi ea) la adresa VIRTUALĂ continuă
                map_page(process_pml4, 0x800000 + (p * 4096), allocated_pages[p], 
                         PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
            }

            // B. Salvăm CR3-ul kernelului și Trecem TEMPORAR pe noua hartă
            uint64_t old_cr3;
            __asm__ volatile("mov %%cr3, %0" : "=r"(old_cr3));
            __asm__ volatile("mov %0, %%cr3" :: "r"((uint64_t)process_pml4));

            // C. Citim fișierul CONTINUU la adresa VIRTUALĂ!
            // Acum procesorul (MMU) va sparge automat cei 32KB și îi va pune în paginile corecte!
            int bytes_read = fs_read_file(paths_to_try[valid_path_idx], (uint8_t*)0x800000,MAX_PROG_PAGES*4096 );
            
            NanoHeader* hdr = (NanoHeader*)0x800000; 

            // Verificăm dacă fișierul e corupt
            if (bytes_read <= (int)sizeof(NanoHeader) || 
                hdr->magic[0] != 'N' || hdr->magic[1] != 'A' || 
                hdr->magic[2] != 'S' || hdr->magic[3] != '1') {
                
                __asm__ volatile("mov %0, %%cr3" :: "r"(old_cr3)); // Revenim la kernel
                for(int p = 0; p < 8; p++) free_page((void*)allocated_pages[p]);
                free_process_paging((uint64_t)process_pml4);
                regs->rax = 0;
                break;
            }

            uint64_t entry_point = 0x800000 + hdr->entry_offset;
            
            // D. Revenim la CR3-ul kernelului ÎNAINTE de a apela alte funcții!
            __asm__ volatile("mov %0, %%cr3" :: "r"(old_cr3));

            // E. Plasăm argumentele (rămâne identic)
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
            
            // F. Creăm procesul și îi pasăm lista celor 8 pagini!
			// PRELUĂM TTY-ul părintelui (sau active_tty dacă nu avem părinte)
            int target_tty = (current_process != 0) ? current_process->tty_id : active_tty;
            uint32_t new_pid = process_create(prog_name, entry_point, argc, (char**)user_argv, (uint64_t)process_pml4, allocated_pages, target_tty);
			
            char debug_buf[32];
/*			
print("[DEBUG] Lansare proces '");
print(prog_name);
print("' pe TTY index: ");
simple_itoa(target_tty, debug_buf);
print(debug_buf);
print("\n");
*/
			
            ttys[target_tty].foreground_pid = new_pid;
            
            regs->rax = (uint64_t) new_pid;
            break;
        }
		
	
		case SYSCALL_EXIT: {
			__asm__ volatile ("cli");
			PCB* exiting_process = current_process;
			if (exiting_process) {
				uint32_t parent_pid = exiting_process->ppid;
				exiting_process->exit_code = (int)regs->rdi;

				// Folosim harta kernelului înainte de eliberarea paginilor procesului
				__asm__ volatile ("mov %0, %%cr3" :: "r"(kernel_cr3));

				for (int p = 0; p < MAX_PROG_PAGES; p++) {
					if (exiting_process->prog_pages[p] != 0) {
						free_page((void*)exiting_process->prog_pages[p]);
						exiting_process->prog_pages[p] = 0;
					}
				}

				if (exiting_process->cr3) {
					free_process_paging(exiting_process->cr3);
					exiting_process->cr3 = 0;
				}

				// 1. Îl facem ZOMBIE (păstrăm PID-ul și ppid-ul ca părintele să-l poată culege prin wait)
				exiting_process->state = PROC_ZOMBIE;
				
				int target_tty = exiting_process->tty_id;

				// 2. Căutăm părintele și îl trezim din somn dacă aștepta
				for (int i = 0; i < MAX_PROCESSES; i++) {
					if (process_table[i].pid == parent_pid &&
						process_table[i].state == PROC_SLEEPING) {
						process_table[i].state = PROC_READY;
						keyboard_flush();
						break;
					}
				}
				
				ttys[target_tty].foreground_pid = parent_pid;
			}

			// 3. Predăm controlul: scoatem procesul curent și forțăm oprirea
			current_process = NULL;
			__asm__ volatile ("sti");

			// Forțăm un hlt până la următorul tic de ceas; 
			// schedulerul va prelua automat un alt proces READY din tabelă.
			while (1) { __asm__ volatile ("hlt"); }
			break;
		}
		
        case SYSCALL_KILL: {
            int target_pid = (int)regs->rdi;
            if (target_pid <= 1) { regs->rax = (uint64_t)-1; break; }

            if (current_process && current_process->pid == (uint32_t)target_pid) {
                regs->rax = (uint64_t)-1;
                break;
            }
            
            int killed = 0;
            for (int i = 0; i < MAX_PROCESSES; i++) {
                PCB* victim = &process_table[i];
                if (victim->state != PROC_FREE && victim->pid == (uint32_t)target_pid) {
                    
                    // Pentru KILL, curățăm resursele procesului mort

                    // A. Eliberăm stiva (Heap)
                    if (victim->stack_base) {
                        free((void*)victim->stack_base);
                        victim->stack_base = 0;
                    }

                    // B. Eliberăm cele 8 pagini FIZICE disjuncte ale binarului
                    for (int p = 0; p < MAX_PROG_PAGES; p++) {
                        if (victim->prog_pages[p] != 0) {
                            free_page((void*)victim->prog_pages[p]);
                            victim->prog_pages[p] = 0;
                        }
                    }

                    // C. Eliberăm toate cele ~19 pagini ale Ierarhiei Paging
                    if (victim->cr3) {
                        free_process_paging(victim->cr3);
                        victim->cr3 = 0;
                    }

                    uint32_t parent_pid = victim->ppid;
                    victim->state = PROC_FREE;
                    victim->pid = 0;
                    victim->name[0] = '\0';
					
					int target_tty = victim->tty_id;;
					
                    for (int parent_idx = 0; parent_idx < MAX_PROCESSES; parent_idx++) {
                        PCB* parent = &process_table[parent_idx];
                        if (parent->pid == parent_pid && parent->state == PROC_SLEEPING) {
                            parent->state = PROC_READY;
							
                            keyboard_flush();
                            break;
                        }
                    }
                    ttys[target_tty].foreground_pid = parent_pid;
					
                    killed = 1;
                    break;
                }
            }
            regs->rax = killed ? 1 : 0;
            break;
        }
		
		
		case SYSCALL_WAIT: {
			int has_children = 0;
			int dead_child_found = 0;
			int dead_child_pid = 0;

			// 1. Scanăm tabela de procese pentru copiii acestui proces
			for (int i = 0; i < MAX_PROCESSES; i++) {
				if (process_table[i].state != PROC_FREE && process_table[i].ppid == current_process->pid) {
					has_children = 1;
					
					// Folosim PROC_ZOMBIE (starea corectă din pcb.h)
					if (process_table[i].state == PROC_ZOMBIE) {
						dead_child_pid = process_table[i].pid;
						
						// Eliberăm slotul copilului terminat
						process_table[i].state = PROC_FREE;
						process_table[i].pid = 0;
						process_table[i].ppid = 0;
						
						dead_child_found = 1;
						break;
					}
				}
			}

			// Dacă nu are deloc copii, returnăm 0
			if (!has_children) {
				regs->rax = 0;
				break;
			}

			// Dacă am găsit un copil deja terminat (zombie), îi returnăm PID-ul imediat!
			if (dead_child_found) {
				regs->rax = dead_child_pid;
				break;
			}

			// 2. Dacă are copii, dar TOȚI rulează încă, punem Shell-ul la somn
			current_process->state = PROC_SLEEPING;
			
			while (current_process->state == PROC_SLEEPING) {
                __asm__ volatile ("sti; hlt" ::: "memory");
			}

			// Când s-a trezit (pentru că un copil a murit), culegem copilul zombie
			for (int i = 0; i < MAX_PROCESSES; i++) {
				if (process_table[i].state == PROC_ZOMBIE && process_table[i].ppid == current_process->pid) {
					dead_child_pid = process_table[i].pid;
					process_table[i].state = PROC_FREE;
					process_table[i].pid = 0;
					process_table[i].ppid = 0;
					regs->rax = dead_child_pid;
					break;
				}
			}
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
            KLOG_INFO("Kernel: Shutdown cerut din User Space...\n");
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
			int c = keyboard_read_char(); // Returnează între 0 și 255
			regs->rax = (uint64_t)(uint8_t)c; // Forțăm cast-ul ca unsigned pe 8 biți
			break;
		}
		case SYSCALL_HAS_CHAR: {
			regs->rax = (uint64_t)keyboard_has_data();
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
		case SYSCALL_SYSLOG: { // 
			char* user_msg = (char*)regs->rdi;
			if (user_msg != NULL) {
				KLOG_INFO(user_msg);
				
			}
			break;
		}
		case SYSCALL_GETLOG: { // SYSCALL_GETLOG
			char* user_dest = (char*)regs->rdi;
			// Copiezi kernel_log_buffer în adresa cerută de user-space
			string_copy(user_dest, kernel_log_buffer);
			break;
		}
		case SYSCALL_PRINT_FLOAT: {
            uint64_t raw_bits = regs->rdi;
            float val = *(float*)&raw_bits;

            int int_part = (int)val;
            int frac_part = (int)((val - int_part) * 100);
            if (frac_part < 0) frac_part = -frac_part;

            print_number(int_part);
            print(".");
            if (frac_part < 10) print("0");
            print_number(frac_part);
            break;
        }
		case SYSCALL_REBOOT:
			kernel_reboot();
			break;
		case SYSCALL_GETPID:
			// Returnează PID-ul procesului care rulează chiar acum pe CPU
			regs->rax = current_process ? (uint64_t)current_process->pid : 0;
			break;
		case SYSCALL_BEEP: {
			uint32_t frequency = (uint32_t)regs->rdi;
			uint32_t duration_ms = (uint32_t)regs->rsi;
			
			beep(frequency, duration_ms); // Funcția ta de speaker din kernel
			
			regs->rax = 0;
			break;
		}
		case SYSCALL_SET_CURSOR_SHAPE: {
			int style = (int)regs->rdi;
			set_cursor_shape(style);
			regs->rax = 0;
			break;
		}
		case SYSCALL_PLAY_AUDIO: {
			const uint8_t* user_data = (const uint8_t*)regs->rdi;
			uint32_t length = (uint32_t)regs->rsi;
			
			// Apelează funcția generală din audio.h / ac97
			play_pcm(user_data, length);
			
			regs->rax = 0; // Succes
			break;
		}
		case SYSCALL_MOUNT:
			// regs->rdi conține primul argument: pointerul către string-ul trimis din user-space (ex: "v3" sau "nan2")
			fs_switch_driver((const char*)regs->rdi);
			regs->rax = 1; // Returnează succes
			break;
		case SYSCALL_FDISK:
			fs_fdisk();
			break;
		// În funcția de rutare a syscall-urilor din kernel:
		case SYSCALL_DISK_STATS: {
			const char* path = (const char*)regs->rdi;
			DiskStats* stats = (DiskStats*)regs->rsi;
			
			if (!path || !stats) {
				regs->rax = (uint64_t)-1;
				break;
			}

			char local_path[128];
			FileSystemInterface* target = vfs_route(path, local_path);
			
			if (!target || !target->get_stats) {
				regs->rax = (uint64_t)-1;
				break;
			}

			uint32_t total = 0, free = 0;
			if (target->get_stats(&total, &free)) {
				stats->total_sectors = total;
				stats->free_sectors = free;
				stats->sector_size = 512;
				regs->rax = 0; // Succes
			} else {
				regs->rax = (uint64_t)-1;
			}
			break;
		}
		case SYSCALL_VIDEO_INFO: {
            VideoModeInfo* user_info = (VideoModeInfo*)regs->rdi;
            if (user_info) {
                // Preluăm starea curentă din kernel prin funcția ta din video.h
                VideoModeInfo* kernel_info = video_get_info();
                if (kernel_info) {
                    *user_info = *kernel_info; // Copiem structura direct în bufferul trimis de user-space
                    regs->rax = 1;            // Succes
                } else {
                    regs->rax = 0;
                }
            } else {
                regs->rax = 0;
            }
            break;
        }
		case SYSCALL_SWAP_VIDEO_BUFFERS: {
			video_swap_buffers();
			regs->rax = 1;
			break;
		}
		// Iar în interiorul switch-ului de syscall-uri (cazul 42):
		case SYSCALL_DRAW_FRAME: {
			uint16_t* user_buffer = (uint16_t*)regs->rdx;
			
			if (user_buffer != 0) {
				int total_cells = term_cols * term_rows;
				for (int i = 0; i < total_cells; i++) {
					ttys[active_tty].screen_buffer[i] = user_buffer[i];
				}
				gfx_redraw_tty(active_tty);
			}
			
			regs->rax = 1;
			break;
		}
		case SYSCALL_GET_DIR_ENTRIES: {
			int index = (int)regs->rdi;
			char* user_buf = (char*)regs->rsi;
			
			// Poți folosi KLOG_INFO sau funcția ta de print din kernel
			//KLOG_INFO("[DEBUG KERNEL] Syscall 43 apelat pentru indexul: %d\n", index);

			int found = fs_get_file_at_index(index, user_buf, 64);
			
			if (found) {
				//KLOG_INFO("[DEBUG KERNEL] -> Gasit fisier: %s\n", user_buf);
				regs->rax = 1;
			} else {
				//KLOG_INFO("[DEBUG KERNEL] -> Nu mai sunt fisiere la indexul \n", index);
				regs->rax = 0;
			}
			break;
		}
		case SYSCALL_FORK: {
			//print("Incer fork!");
			//sleep_ms(2000);
			uint32_t child_pid = sys_fork(regs);
			
			// În x86-64, valoarea returnată de syscall se pune în rax pentru părinte.
			// Pentru copil, sys_fork a setat deja explicit rax = 0 în stiva lui privată!
			regs->rax = (uint64_t)child_pid;
			break;
		}
		default: {
			char log_msg[128];
			snprintf(log_msg, sizeof(log_msg), "Syscall necunoscut: RAX=%x, RDI=%p", regs->rax, (void*)regs->rdi);
			KLOG_WARNING(log_msg);
            break;
        }
    }
	
	// 2. Reactivăm întreruperile la ieșire
    __asm__ volatile("sti");
}
