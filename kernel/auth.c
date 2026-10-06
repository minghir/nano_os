#include "auth.h"
#include "pcb.h"

extern PCB* current_process;

uint32_t get_uid() {
    if (current_process) {
        return current_process->uid;
    }
    return 0; // Fallback la root dacă nu există context
}