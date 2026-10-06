#include "nano_libc.h" // Sau cum se numește header-ul tău user-space

int main(int argc, char** argv) {
    ProcessInfo procs[32]; // Cerem detalii pentru maxim 32 de procese
    
    int count = nano_get_processes(procs, 32);
    nano_print("\n");
    nano_print("PID   PPID  STATE       NAME\n");
    nano_print("---------------------------------\n");
    
    for (int i = 0; i < count; i++) {
        nano_print_int(procs[i].pid);
        nano_print("    ");
        nano_print_int(procs[i].ppid);
        nano_print("    ");
        
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
    
    //return 0; // Se va transforma în SYSCALL_EXIT magic prin crt0.asm!
    nano_sys_exit(0); // Ieșim din program cu codul 0
}