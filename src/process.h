#ifndef PROCESS_H
#define PROCESS_H

#include "pcb.h"

#define MAX_PROCESSES 64

// Exportăm variabilele globale ca să le poată folosi și syscall.c
extern PCB process_table[MAX_PROCESSES];
extern PCB* current_process;

void process_init();
void process_create(const char* name, uint64_t entry_point, int argc, char** argv);

#endif