#ifndef PROCESS_H
#define PROCESS_H

#include "pcb.h"

#define MAX_PROCESSES 64

// Exportăm variabilele globale ca să le poată folosi și syscall.c
extern PCB process_table[MAX_PROCESSES];
extern PCB* current_process;
extern uint64_t kernel_cr3;

void process_init();
void process_create(const char* name, uint64_t entry_point, int argc, char** argv, uint64_t process_cr3, uint64_t* prog_pages, int tty_id);

#endif