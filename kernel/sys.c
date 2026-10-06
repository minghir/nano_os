#include <stdint.h>

#include "sys.h"
#include "io.h"
#include "process.h"
#include "memory.h"
#include "paging.h"
#include "tty.h"
#include "string.h"

extern uint32_t next_pid;

void kernel_reboot() {
    print("System rebooting...\n");
    
    // 1. Încercăm resetul prin controlerul de tastatură
    outb(0x64, 0xFE);
    
    // 2. Metoda infailibilă pe x86: Triple Fault (Forțează resetul hardware)
    __asm__ volatile ("cli"); // Oprim întreruperile
    
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) idt_zero = { 0, 0 };

    // Încărcăm un IDT complet gol (limită 0)
    __asm__ volatile ("lidt %0" :: "m"(idt_zero));
    
    // Generăm o excepție intenționată (breakpoint / int 3). 
    // Deoarece IDT-ul e gol, procesorul va genera un Fault, apoi un Double Fault, 
    // și în final un Triple Fault care va reseta instant QEMU!
    __asm__ volatile ("int $3");

    // 3. Plasă de siguranță supremă (dacă emularea ignoră triple fault-ul)
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

PCB* get_process_by_pid(uint32_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROC_FREE && process_table[i].pid == pid) {
            return &process_table[i];
        }
    }
    return NULL; // Nu a fost găsit
}


void kill_process_by_pid(uint32_t pid) {
    __asm__ volatile ("cli");
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == pid) {
            uint32_t ppid = process_table[i].ppid;
            int tty_id = process_table[i].tty_id;

            // 1. Eliberăm stiva alocată cu malloc (Curățăm resursele copilului mai întâi)
            if (process_table[i].stack_base) {
                free((void*)process_table[i].stack_base);
                process_table[i].stack_base = 0;
            }
			
			 __asm__ volatile ("mov %0, %%cr3" :: "r"(kernel_cr3));
			
            // 2. Eliberăm paginile fizice și paginarea (PML4)
            for (int p = 0; p < MAX_PROG_PAGES; p++) {
                if (process_table[i].prog_pages[p]) {
                    free_page((void*)process_table[i].prog_pages[p]);
                    process_table[i].prog_pages[p] = 0;
                }
            }
            if (process_table[i].cr3) {
                free_process_paging(process_table[i].cr3);
                process_table[i].cr3 = 0;
            }

            // 3. Eliberăm slotul procesului
            process_table[i].state = PROC_FREE;
            process_table[i].pid = 0;
            process_table[i].name[0] = '\0';

            // 4. Trezim părintele (după ce copilul a fost complet șters din sistem)
            for (int j = 0; j < MAX_PROCESSES; j++) {
                if (process_table[j].pid == ppid) {
                    if (process_table[j].state == PROC_SLEEPING) {
                        process_table[j].state = PROC_READY;
                        keyboard_flush();
                    }
                    break;
                }
            }

            // 5. Redăm foreground-ul părintelui pe TTY
            ttys[tty_id].foreground_pid = ppid;

            break;
        }
    }
}

// Creează o copie completă a paginilor de program ale unui proces
uint64_t* fork_process_memory(uint64_t* parent_prog_pages, uint64_t* new_child_pml4) {
    // Alocăm un nou array pentru paginile copilului
    uint64_t* child_pages = (uint64_t*)malloc(MAX_PROG_PAGES * sizeof(uint64_t));
    if (!child_pages) return NULL;

    for (int i = 0; i < MAX_PROG_PAGES; i++) {
        if (parent_prog_pages[i] != 0) {
            // 1. Alocăm o pagină fizică nouă pentru copil
            void* new_phys_page = alloc_page();
            if (!new_phys_page) {
                // Dacă dă eroare, ar trebui să eliberăm ce am alocat până acum (gestionare simplă de eroare)
                return NULL;
            }

            // 2. Copiem conținutul paginii fizice a părintelui în noua pagină a copilului
            // Notă: Putem folosi un memcpy temporar mapând-o sau accesând direct adresele fizice/virtuale
            // Presupunând că ai o funcție de copiere sau poți folosi maparea temporară:
            uint8_t* src = (uint8_t*)parent_prog_pages[i]; // sau adresa virtuală corespunzătoare
            uint8_t* dst = (uint8_t*)new_phys_page;
            for (int b = 0; b < 4096; b++) {
                dst[b] = src[b];
            }

            child_pages[i] = (uint64_t)new_phys_page;

            // 3. Mapăm noua pagină fizică în noul PML4 al copilului la aceeași adresă virtuală (ex: 0x800000 + i*4096)
            map_page(new_child_pml4, 0x800000 + (i * 4096), (uint64_t)new_phys_page, 
                     PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        } else {
            child_pages[i] = 0;
        }
    }
    return child_pages;
}

uint32_t sys_fork(Registers* parent_regs) {
    if (!current_process) return 0;
    
    print("[FORK DEBUG] Incepem clonarea pentru PID: ");
    print_number(current_process->pid);
    print("\n");

    // 1. Căutăm slot liber
    PCB* child = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROC_FREE) {
            child = &process_table[i];
            break;
        }
    }
    if (!child) {
        print("[FORK ERROR] Tabela de procese e plina!\n");
        return 0;
    }

    // 2. Setăm datele de bază
    child->pid = next_pid++;
    child->ppid = current_process->pid;
    child->tty_id = current_process->tty_id;
    
    int n = 0;
    while (current_process->name[n] && n < 31) { child->name[n] = current_process->name[n]; n++; }
    child->name[n] = '\0';

    child->cwd_sector = current_process->cwd_sector;
    for (int i = 0; i < 256; i++) child->current_path[i] = current_process->current_path[i];

    // 3. CLONAREA MEMORIEI DE PROGRAM
    print("[FORK DEBUG] Clonam paginile de program...\n");
    uint64_t* child_pml4 = create_process_pml4();
    if (!child_pml4) {
        print("[FORK ERROR] Esec la crearea PML4!\n");
        return 0;
    }
    child->cr3 = (uint64_t)child_pml4;

    for (int i = 0; i < MAX_PROG_PAGES; i++) {
        if (current_process->prog_pages[i] != 0) {
            uint64_t new_phys = (uint64_t)alloc_page();
            child->prog_pages[i] = new_phys;

            uint8_t* src = (uint8_t*)(0x800000 + (i * 4096)); 
            uint8_t* dst = (uint8_t*)new_phys;                
            
            for (int b = 0; b < 4096; b++) dst[b] = src[b];

            map_page(child_pml4, 0x800000 + (i * 4096), new_phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        } else {
            child->prog_pages[i] = 0;
        }
    }

    // 4. CLONAREA ȘI RELOCAREA STIVEI
    print("[FORK DEBUG] Clonam stiva (16 KB)...\n");
    
    uint64_t parent_stack_start = current_process->stack_base;
    uint64_t parent_stack_end = parent_stack_start + 16384;

    // SANITY CHECK CRITIC: Registrele părintelui TREBUIE să fie pe stiva lui!
    if ((uint64_t)parent_regs < parent_stack_start || (uint64_t)parent_regs >= parent_stack_end) {
        print("[FORK FATAL] parent_regs NU este pe stiva procesului! Offset invalid.\n");
        return 0;
    }

    uint8_t* raw_child_stack = (uint8_t*)malloc(16384 + 16);
    if (!raw_child_stack) {
        print("[FORK ERROR] Esec malloc stiva copil!\n");
        return 0;
    }
    child->stack_base = (uint64_t)raw_child_stack; 

    // Aliniem manual stiva copilului ca să aibă exact același rest (modulo 16) ca stiva părintelui
    uint64_t child_start = (uint64_t)raw_child_stack;
    while ((child_start & 0xF) != (parent_stack_start & 0xF)) {
        child_start++;
    }
    uint8_t* child_stack_aligned = (uint8_t*)child_start;
    uint8_t* parent_stack_ptr = (uint8_t*)parent_stack_start;

    // Copiem octet cu octet
    for (int i = 0; i < 16384; i++) {
        child_stack_aligned[i] = parent_stack_ptr[i];
    }
    
    // Relocăm toți pointerii interni (ex: RBP, RSP salvate)
    print("[FORK DEBUG] Relocam pointerii din stiva...\n");
    int64_t stack_offset = (int64_t)child_stack_aligned - (int64_t)parent_stack_start;
    uint64_t* child_stack_qwords = (uint64_t*)child_stack_aligned;
    
    for (int i = 0; i < (16384 / 8); i++) {
        if (child_stack_qwords[i] >= parent_stack_start && child_stack_qwords[i] <= parent_stack_end) {
            child_stack_qwords[i] += stack_offset;
        }
    }
    
    // Găsim structura Registers în noua stivă
    uint64_t regs_offset = (uint64_t)parent_regs - parent_stack_start;
    Registers* child_regs = (Registers*)(child_start + regs_offset);
    
    // 5. MOMENTUL MAGIC: Setăm RAX = 0 pentru copil
    child_regs->rax = 0;
    
    // Setăm RSP-ul final pentru scheduler
    child->regs = *child_regs; 
    child->regs.rsp = (uint64_t)child_regs;
    
    child->state = PROC_READY;
    
    print("[FORK SUCCESS] Copil creat cu PID: ");
    print_number(child->pid);
    print("\n");
    
    return child->pid;
}


uint32_t thread_create(uint64_t entry_point, void* arg) {
    PCB* p = 0;
    
    // 1. Căutăm un slot liber în tabela unificată de task-uri
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROC_FREE) {
            p = &process_table[i];
            p->tty_id = current_process ? current_process->tty_id : 0;
            break;
        }
    }
    
    if (!p) {
        print("[DEBUG] ERROR: Process table full! Cannot create thread.\n");
        return 0;
    }

    // 2. MOȘTENIREA RESURSELOR (Magia thread-urilor):
    // Thread-ul folosește EXACT ACEEAȘI memorie virtuală (CR3) și aceleași pagini ca procesul curent!
    p->cr3 = current_process ? current_process->cr3 : kernel_cr3;
    for (int i = 0; i < MAX_PROG_PAGES; i++) {
        p->prog_pages[i] = current_process ? current_process->prog_pages[i] : 0;
    }
    
    // Populăm datele de identificare
    p->pid = next_pid++;
    p->ppid = current_process ? current_process->pid : 0;
    
    string_copy(p->name, current_process ? current_process->name : "thread");
    
    p->cwd_sector = current_process ? current_process->cwd_sector : 1;
    if (current_process) {
        string_copy(p->current_path, current_process->current_path);
    } else {
        p->current_path[0] = '/';
        p->current_path[1] = '\0';
    }

    // 3. Alocăm o stivă privată dedicată acestui thread (ex: 8 KB)
    uint8_t* stack = (uint8_t*)malloc(8192);
    if (!stack) {
        print("[DEBUG] ERROR: malloc failed for thread stack!\n");
        p->state = PROC_FREE;
        return 0;
    }
    p->stack_base = (uint64_t)stack;
    
    // 4. Aliniem vârful stivei la 16 octeți
    uint64_t stack_top = (uint64_t)(stack + 8192);
    stack_top &= ~0xF; 

    // 5. Pregătim structura de registre la vârful stivei
    stack_top -= sizeof(Registers);
    Registers* regs = (Registers*)stack_top;

    // Zeroizăm registrele
    uint8_t* byte_ptr = (uint8_t*)regs;
    for (uint32_t i = 0; i < sizeof(Registers); i++) {
        byte_ptr[i] = 0;
    }

    // 6. Setăm starea pentru IRETQ
    // (Folosim aceleași segmente ca și procesul curent - ex: Ring 0 sau Ring 3)
    regs->ss = 0x10;          
    regs->rsp = stack_top + sizeof(Registers);  
    regs->rflags = 0x202;     // Întreruperi activate (IF=1)
    regs->cs = 0x08;          
    regs->rip = entry_point;  

    // 7. Putem pasa un argument prin RDI (convenție standard)
    regs->rdi = (uint64_t)arg;

    // 8. Salvăm RSP-ul în PCB
    p->regs.rsp = stack_top;
    
    // 9. Îl punem în starea READY, gata de rulare de către scheduler
    p->state = PROC_READY;
    
    print("[THREAD_CREATE] Thread creat cu succes | TID: ");
    print_number(p->pid);
    print("\n");

    return p->pid; 
}