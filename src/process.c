#include "process.h"
#include "memory.h"
#include "string.h"
#include "io.h"


PCB process_table[MAX_PROCESSES];
PCB* current_process = 0;
uint32_t next_pid = 1;
uint64_t kernel_cr3 = 0;

void process_init() {
	// Salvăm CR3-ul de bază al Kernelului O SINGURĂ DATĂ la boot
    __asm__ volatile("mov %%cr3, %0" : "=r"(kernel_cr3));
	
	//salva adresa PML4 globală a kernelului la pornire și să o atribui procesului 0 în
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
    
    print("[DEBUG] process_init: Kernel process (PID 0) initialized as RUNNING.\n");
}

// Planificatorul (Scheduler-ul) apelat la fiecare milisecundă
/*
uint64_t schedule(uint64_t current_rsp) {
	//print(".");
    if (!current_process) return current_rsp;

    // 1. Salvăm RSP-ul curent în PCB-ul procesului care tocmai rulează
    current_process->regs.rsp = current_rsp;

    // 2. Găsim următorul proces READY
    int start_idx = (current_process - process_table) + 1;
    PCB* next_proc = NULL;
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        int idx = (start_idx + i) % MAX_PROCESSES;
        //if (process_table[idx].state == PROC_READY) {
		if (process_table[idx].state == PROC_READY && process_table[idx].pid != 0) {
            next_proc = &process_table[idx];
			
			if (next_proc->pid == 1) {
				// Punem un mesaj o singură dată când scheduler-ul trece pe shell pentru prima oară
				static int shell_switched = 0;
				if (!shell_switched) {
					print("[DEBUG] Scheduler switched to Shell (PID 1) for the first time!\n");
					shell_switched = 1;
				}
			}
			
            break;
        }
    }

    if (!next_proc) {
        // Dacă nu găsim nimic altceva gata de rulare, rămânem pe cel curent
        return current_rsp; 
    }

    // [DEBUG] Dacă s-a găsit un proces nou (ex: Shell-ul), afișăm o singură dată sau la schimbare
    // (Atenție: print-ul în interiorul scheduler-ului poate încetini sistemul dacă rulează la 1000Hz, 
    // dar e excelent acum pentru a vedea dacă timer-ul declanșează switch-ul!)
    if (next_proc != current_process) {
        // Poți decomenta linia de jos dacă vrei să vezi fiecare switch:
        // print("[SCHED] Switching from PID to new process\n");
    }

    // 3. Facem switch-ul "logic"
    if (current_process->state == PROC_RUNNING) {
        current_process->state = PROC_READY; 
    }
    next_proc->state = PROC_RUNNING;
    current_process = next_proc;

	// 3.1 --- Schimbăm memoria virtuală (CR3) dacă procesul are una ---
    if (current_process->cr3 != 0) {
        __asm__ volatile("mov %0, %%cr3" :: "r"(current_process->cr3));
    }

    // 4. Returnăm noul RSP
    return current_process->regs.rsp;
}
*/

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
    if (current_process->cr3 != 0) {
        __asm__ volatile("mov %0, %%cr3" :: "r"(current_process->cr3));
    }

    return current_process->regs.rsp;
}

void process_create(const char* name, uint64_t entry_point, int argc, char** argv, uint64_t process_cr3, uint64_t* prog_pages)  {
    
    //print("[DEBUG] process_create: Trying to create process '");
    //print(name);
    //print("'...\n");
	
    PCB* p = 0;
    
    // 1. Căutăm un slot liber în tabelă
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROC_FREE) {
            p = &process_table[i];
            //print("[DEBUG] process_create: Found free slot at index ");
            // Dacă vrei poți afișa și indexul
            //print("\n");
            break;
        }
    }
    
    if (!p) {
        print("[DEBUG] ERROR: Process table full! Cannot create process.\n");
        return;
    }

    // An exited process keeps its stack until the scheduler has switched away.
    if (p->stack_base) {
        free((void*)p->stack_base);
        p->stack_base = 0;
    }

	// Îi asociezi harta de memorie unică
    p->cr3 = process_cr3;
	
	// Salvăm cele 8 pagini împrăștiate
    for(int i = 0; i < 8; i++) {
        p->prog_pages[i] = prog_pages[i];
    }
	
    // 2. Populăm datele de bază
    p->pid = next_pid++;
    p->ppid = current_process ? current_process->pid : 0;
    
    int n = 0;
    while (name[n] && n < 31) { p->name[n] = name[n]; n++; }
    p->name[n] = '\0';
    
    p->cwd_sector = current_process ? current_process->cwd_sector : 1;

    // 3. Alocăm stiva privată a programului (16 KB)
    uint8_t* stack = (uint8_t*)malloc(16384);
    if (!stack) {
        print("[DEBUG] ERROR: malloc failed for process stack!\n");
        return;
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

    // 6. Setăm starea procesorului pentru IRETQ
    regs->ss = 0x10;        
    regs->rsp = stack_top + sizeof(Registers); // <-- Punctează fix la vârful aliniat!
    regs->rflags = 0x202;     // IF=1 (Întreruperi activate)
    regs->cs = 0x08;          
    regs->rip = entry_point;  

    // 7. Parametrii argc / argv
    regs->rdi = (uint64_t)argc;
    regs->rsi = (uint64_t)argv;

    // 8. Salvăm RSP-ul final în PCB
    p->regs.rsp = stack_top;
	
    // 9. Îl marcăm ca pregătit să ruleze
    p->state = PROC_READY;
    
    //print("[DEBUG] process_create: Process created successfully! PID assigned, state set to PROC_READY.\n");
}

