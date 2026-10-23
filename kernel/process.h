#ifndef PROCESS_H
#define PROCESS_H

#include "pcb.h"

#define MAX_PROCESSES 512

// Exportăm variabilele globale ca să le poată folosi și syscall.c
extern PCB process_table[MAX_PROCESSES];
extern PCB* current_process;
extern uint64_t kernel_cr3;

void process_init();
uint32_t process_create(const char* name, uint64_t entry_point, int argc, char** argv, uint64_t process_cr3, uint64_t* prog_pages, uint64_t args_page, int tty_id);
void process_release_owned_pages(PCB* process);

#endif