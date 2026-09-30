#ifndef PCB_H
#define PCB_H

#include <stdint.h>

//PCB (Process Control Block)

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
    
    // --- Memorie și Execuție ---
    Registers regs;           // Starea registrelor când e pus pe pauză
    uint64_t cr3;             // Paging (dacă folosești memorie virtuală)
    uint64_t mem_base;        // De unde începe în memorie (ex: 0x900000)
    uint64_t stack_base;      // Baza stivei procesului
    
    // --- Timp și Statistici ---
    uint64_t start_time;      // Ticks la care a pornit
    uint64_t cpu_time;        // Cât timp a stat efectiv pe CPU
    
    // --- Control ---
    int exit_code;            // Codul returnat la final (ex: return 0 din main)
} PCB;
#endif