#ifndef PCB_H
#define PCB_H

#include <stdint.h>

//PCB (Process Control Block)
#define MAX_PROG_PAGES 16
// Stările posibile ale unui proces
typedef enum {
    PROC_FREE = 0,    // Slot liber în memorie
    PROC_RUNNING,     // Rulează chiar acum pe procesor
    PROC_READY,       // Gata să ruleze, așteaptă rândul la CPU
    PROC_SLEEPING,    // Așteaptă ceva (ex: citire de la tastatură, sleep())
    PROC_ZOMBIE       // S-a terminat, dar părintele încă nu i-a citit exit_code-ul
} ProcessState;

// Structură pentru a salva starea procesorului (Cea mai importantă parte!)
// Structură care mapează EXACT push-urile din isr_timer.asm
typedef struct {
    // Salvate de: sub rsp, 8 / sub rsp, 16 / movdqu
    uint8_t xmm0[16];
    uint64_t padding;

    // Ordinea inversă a pop-urilor / push-urilor (r15 e la vârful stivei)
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;

    // Puse automat de procesor (CPU IRET frame) când apare întreruperea
    uint64_t rip;     // Instruction Pointer
    uint64_t cs;      // Code Segment
    uint64_t rflags;  // Flags (EFLAGS)
    uint64_t rsp;     // Stack Pointer original
    uint64_t ss;      // Stack Segment
} __attribute__((packed)) Registers;

// Structura principală a procesului (PCB)
typedef struct {
    uint32_t pid;             // Process ID (ex: 1, 2, 3...)
    uint32_t ppid;            // Parent PID (Cine l-a creat, ex: shell-ul)
    char name[32];            // Numele (ex: "vi", "shell", "mandel")
    
    ProcessState state;       // Starea curentă
    
    // --- Sistem de Fișiere ---
    uint32_t cwd_sector;      // Current Working Directory! (Înlocuiește variabila globală)
	char current_path[256];     // Calea text privată a procesului (ex: "/hda/data")
    
	// --- User si Group
	
	uint32_t uid;       // User ID-ul proprietarului (ex: 0 = root, 1000 = user normal)
    uint32_t gid;       // Group ID-ul principal
    uint32_t euid;      // Effective UID (opțional, util pentru setuid binaries pe viitor)
	
    // --- Memorie și Execuție ---
    Registers regs;           // Starea registrelor când e pus pe pauză
    uint64_t cr3;             // Paging (dacă folosești memorie virtuală)
    uint64_t mem_base;        // De unde începe în memorie (ex: 0x900000)
    uint64_t stack_base;      // Baza stivei procesului
    uint64_t prog_pages[MAX_PROG_PAGES];	  // Procesul nu mai are un singur pointer de memorie continuă, ci un array de 8 pagini: Adresa fizică a programului (32KB)
	
    // --- Timp și Statistici ---
    uint64_t start_time;      // Ticks la care a pornit
    uint64_t cpu_time;        // Cât timp a stat efectiv pe CPU
    
    // --- Control ---
    int exit_code;            // Codul returnat la final (ex: return 0 din main)
	int tty_id;               // TTY-ul de care aparține procesul
	
	uint32_t waiting_for_tid;   // ID-ul thread-ului pe care acest task îl așteaptă (prin join)
} PCB;
#endif