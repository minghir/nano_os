#include "process.h"
#include "memory.h"
#include "paging.h"
#include "string.h"
#include "io.h"
#include "tty.h"
#include "tss.h"
#include "syslog.h"


PCB process_table[MAX_PROCESSES];
PCB* current_process = 0;
uint32_t next_pid = 1;
uint64_t kernel_cr3 = 0;

void process_init() {
    // Salvăm CR3-ul de bază al Kernelului O SINGURĂ DATĂ la boot
    __asm__ volatile("mov %%cr3, %0" : "=r"(kernel_cr3));
    
    uint64_t kernel_pml4;
    __asm__ volatile("mov %%cr3, %0" : "=r"(kernel_pml4));
    
    print("[DEBUG] process_init: Cleaning process table...\n");
    // 1. Curățăm toate sloturile din tabelă
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].state = PROC_FREE;
        process_table[i].pid = 0;
    }

    // 2. Creăm procesul "Părinte Suprem" (Kernelul însuși)
    current_process = &process_table[0];
    current_process->pid = 0;
    current_process->ppid = 0;
    current_process->state = PROC_RUNNING;
    current_process->cr3 = kernel_pml4;
    current_process->cwd_sector = 1; 
    current_process->tty_id = active_tty;
	
	// --- Inițializare redirecționare STDOUT pentru Kernel ---
    current_process->has_stdout_redirect = 0;
    current_process->stdout_file[0] = '\0';
    
    // --- FIX: Inițializăm calea kernelului la root ---
    current_process->current_path[0] = '/';
    current_process->current_path[1] = '\0';
    
    print("[DEBUG] process_init: Kernel process (PID 0) initialized as RUNNING.\n");
}

// Planificatorul (Scheduler-ul) apelat la fiecare milisecundă
uint64_t schedule(uint64_t current_rsp) {
    // 1. SALVĂM ÎNTOTDEAUNA stiva procesului curent întrerupt de timer,
    // indiferent dacă era RUNNING sau SLEEPING!
    if (current_process) {
        current_process->regs.rsp = current_rsp;
    }

    // 2. Găsim următorul proces READY
    int start_idx = current_process ? ((current_process - process_table) + 1) : 0;
    
    PCB* next_proc = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        int idx = (start_idx + i) % MAX_PROCESSES;
        
        // Sărim peste Kernel (PID 0) pentru programele normale
        if (process_table[idx].state == PROC_READY && process_table[idx].pid != 0) {
            next_proc = &process_table[idx];
            break;
        }
    }

    // 3. Dacă nu am găsit nimic din USER SPACE...
    if (!next_proc) {
        if (!current_process) {
            // Plasa de siguranță: forțăm întoarcerea pe Kernel (PID 0)
            next_proc = &process_table[0];
        } else {
            // Nu e nimeni altcineva gata. Rămânem pe cel curent!
            return current_rsp; 
        }
    }

    // 4. Facem switch-ul efectiv (Schimbăm stările)
    if (current_process && current_process->state == PROC_RUNNING) {
        current_process->state = PROC_READY; 
    }
    
    next_proc->state = PROC_RUNNING;
    current_process = next_proc;

    // 5. Schimbăm memoria virtuală (CR3)
    //if (current_process->cr3 != 0) {
	//	__asm__ volatile("mov %0, %%cr3" :: "r"(current_process->cr3));
    //}
	// Oprește rescrierea redundantă a CR3-ului între thread-uri ale aceluiași proces
    if (next_proc->cr3 != 0) {
        uint64_t old_cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(old_cr3));
        if (old_cr3 != next_proc->cr3) {
            __asm__ volatile("mov %0, %%cr3" :: "r"(next_proc->cr3));
        }
    }
	
	// ======== ADAUGĂ ASTA AICI ========
    // Îi spunem procesorului ca la următoarea întrerupere din Ring 3, 
    // să sară pe stiva privată de KERNEL alocată prin malloc a noului proces!
    kernel_tss.rsp0 = next_proc->stack_base + 16384; 
    // ==================================
	static int first_switch = 1;
    if (first_switch && current_process && current_process->pid == 1) {
        KLOG_INFO("\n[SCHEDULER] BINGO! Am comutat pe PID 1 (Init) in Ring 3!\n");
		char buf[32];
        
        KLOG_INFO("RIP (Adresa cod): "); simple_itoa(next_proc->regs.rip, buf); KLOG_INFO(buf); KLOG_INFO("\n");
        KLOG_INFO("RSP (Stiva): "); simple_itoa(next_proc->regs.rsp, buf); KLOG_INFO(buf); KLOG_INFO("\n");
        KLOG_INFO("RFLAGS (Status): "); simple_itoa(next_proc->regs.rflags, buf); KLOG_INFO(buf); KLOG_INFO("\n");
        KLOG_INFO("CS (Selector): "); simple_itoa(next_proc->regs.cs, buf); KLOG_INFO(buf); KLOG_INFO("\n");
        first_switch = 0;
    }

    return current_process->regs.rsp;
}

void dummy_ring3_loop() {
    // O buclă infinită care rulează direct în Ring 3!
    while(1); 
}

uint32_t process_create(const char* name, uint64_t entry_point, int argc, char** argv, uint64_t process_cr3, uint64_t* prog_pages, int tty_id)  {
    
    PCB* p = 0;
    
    // 1. Căutăm un slot liber în tabelă
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROC_FREE) {
            p = &process_table[i];
            process_table[i].tty_id = tty_id;
            break;
        }
    }
    
    if (!p) {
        print("[DEBUG] ERROR: Process table full! Cannot create process.\n");
        return 0;
    }

    // An exited process keeps its stack until the scheduler has switched away.
    if (p->stack_base) {
        free((void*)p->stack_base);
        p->stack_base = 0;
    }

    // Îi asociezi harta de memorie unică
    p->cr3 = process_cr3;
    
    // Salvăm cele 8 pagini împrăștiate
    for(int i = 0; i < MAX_PROG_PAGES; i++) {
        p->prog_pages[i] = prog_pages[i];
    }
    
    // 2. Populăm datele de bază
    p->pid = next_pid++;
    p->ppid = current_process ? current_process->pid : 0;
    
    int n = 0;
    while (name[n] && n < 31) { p->name[n] = name[n]; n++; }
    p->name[n] = '\0';
    
    p->cwd_sector = current_process ? current_process->cwd_sector : 0;
    
	
	// --- INIȚIALIZARE REDIRECȚIONARE STDOUT ---
    p->has_stdout_redirect = 0;
    p->stdout_file[0] = '\0';
    // ------------------------------------------
	
	
    // Copierea căii text a procesului părinte
    if (current_process && current_process->current_path[0] != '\0') {
        int idx = 0; // am pus 0
        while (current_process->current_path[idx] && idx < 255) {
            p->current_path[idx] = current_process->current_path[idx];
            idx++;
        }
        p->current_path[idx] = '\0';
    } else {
        p->current_path[0] = '/';
        p->current_path[1] = '\0';
    }

	// În process_create:
	p->uid = current_process ? current_process->uid : 0; // Root by default dacă e init
	p->gid = current_process ? current_process->gid : 0;


    // --- LOGURI DE DIAGNOSTIC PENTRU PROCES ȘI CALE ---
    //print("[PROC_CREATE] Nume: ");
    //print(p->name);
    //print(" | PID: ");
    //print_number(p->pid);
    //print(" | PPID: ");
    //print_number(p->ppid);
    //print(" | CWD Inod: ");
    //print_number(p->cwd_sector);
    //print(" | Cale: ");
    //print(p->current_path);
    //print("\n");
    // --------------------------------------------------
    
    // 3. Alocăm stiva privată a programului (16 KB)
    uint8_t* stack = (uint8_t*)malloc(16384);
    if (!stack) {
        print("[DEBUG] ERROR: malloc failed for process stack!\n");
        return 0;
    }
    p->stack_base = (uint64_t)stack;
    
    // 4. Calculăm Vârful stivei și o aliniem la 16 octeți
    uint64_t stack_top = (uint64_t)(stack + 16384);
    stack_top &= ~0xF; 

    // 5. Facem loc pentru structura Registers exact la vârful stivei
    stack_top -= sizeof(Registers);
    Registers* regs = (Registers*)stack_top;

    // Zeroizăm tot struct-ul de registre
    uint8_t* byte_ptr = (uint8_t*)regs;
    for (uint32_t i = 0; i < sizeof(Registers); i++) {
        byte_ptr[i] = 0;
    }

// 6. Setăm starea procesorului pentru IRETQ (User Space - Ring 3)
/*
    regs->ss = 0x10;          // Selector de date User Mode (Ring 3, ex: 0x20 | 3)
    regs->rsp = stack_top + sizeof(Registers);  
    regs->rflags = 0x202;     // IF=1 (Întreruperi activate)
    regs->cs = 0x08;          // Selector de cod User Mode (Ring 3, ex: 0x18 | 3)
    regs->rip = entry_point;
*/	





// 1. Alocăm fizic pagina pentru stiva de user
    uint64_t user_stack_phys = (uint64_t)alloc_page();
    
    // 2. O mapăm în PML4-ul procesului (de exemplu la adresa virtuală 0x900000)
    // Asigură-te că pui flag-ul PAGE_USER ca Ring 3 să aibă drept de acces!
    // 2. O mapăm în PML4-ul procesului
	map_page((uint64_t*)process_cr3, 0x900000, user_stack_phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

    // 3. Setăm segmentele pentru Ring 3
    regs->ss = 0x23;             // Selector de stivă User (Ring 3 Data)
    
    // 4. AICI ESTE SCHIMBAREA CRUCIALĂ: 
    // RSP-ul de user trebuie să indice spre vârful zonei mapate (0x900000 + 4096 = 0x901000)
    regs->rsp = 0x901000;        
    
    regs->rflags = 0x202;        // Întreruperi activate (IF=1)
    regs->cs = 0x1b;             // Selector de cod User (Ring 3 Code)
    regs->rip = entry_point;     // Punctul de intrare în binarul tău (.nas)
	//regs->rip = (uint64_t)dummy_ring3_loop;
	






    // 7. Parametrii argc / argv
    regs->rdi = (uint64_t)argc;
    regs->rsi = (uint64_t)argv;

    // 8. Salvăm RSP-ul final în PCB
    p->regs.rsp = stack_top;
    
    // 9. Îl marcăm ca pregătit să ruleze
    p->state = PROC_READY;
    
    return p->pid; 
}

