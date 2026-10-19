#include "auth.h"
#include "pcb.h"

extern PCB* current_process;

UserAccount user_database[MAX_USERS] = {
    { "nano", 0, 0, "/hda" },
    { "user", 1000, 1000, "/hda/home/user" }
};

uint32_t get_uid(void) {
    // Returnează UID-ul procesului curent
    extern PCB* current_process;
    if (current_process) {
        return current_process->uid;
    }
    return 0; // Fallback pe root/nano
}