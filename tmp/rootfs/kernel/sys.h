#ifndef SYS_H
#define SYS_H

#include "process.h"

void kernel_reboot();
void kill_process_by_pid(uint32_t pid);
uint32_t sys_fork(Registers* parent_regs);
PCB* get_process_by_pid(uint32_t pid);
uint32_t thread_create(uint64_t entry_point, void* arg);

#endif