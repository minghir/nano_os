#include "nano_libc.h"

// Funcție auxiliară în ps.c pentru a traduce UID-ul în nume
const char* get_username_by_uid(uint32_t uid) {
    switch (uid) {
        case 0:    return "root";
        case 1000: return "user";
        default:   return "unknown";
    }
}

int main(int argc, char** argv) {
    ProcessInfo procs[32]; 
    
    int count = nano_get_processes(procs, 32);
    nano_print("\n");
    nano_print("PID   PPID  UID   STATE      NAME\n");
    nano_print("-------------------------------------\n");
    
    for (int i = 0; i < count; i++) {
        nano_print_int(procs[i].pid);
        nano_print("    ");
        
        nano_print_int(procs[i].ppid);
        nano_print("    ");
        
        // În loc de UID numeric, afișăm numele utilizatorului!
        const char* uname = get_username_by_uid(procs[i].uid);
        nano_print(uname);
        // Adăugăm un pic de padding ca să se alinieze frumos în tabelă
        for (int p = 0; p < (7 - (int)__builtin_strlen(uname)); p++) nano_print(" ");
        
        // Starea...
        switch (procs[i].state) {
            case 1: nano_print("RUNNING   "); break;
            case 2: nano_print("READY     "); break;
            case 3: nano_print("SLEEPING  "); break;
            case 4: nano_print("ZOMBIE    "); break;
            default: nano_print("UNKNOWN   "); break;
        }
        
        nano_print(procs[i].name);
        nano_print("\n");
    }
    
    nano_sys_exit(0);
}