#include "nano_libc.h"

int main(int argc, char** argv) {
    ProcessInfo procs[32]; 
    int count = nano_get_processes(procs, 32);

    nano_print("\nPID    PPID    USER       STATE      NAME\n");
    nano_print("-------------------------------------\n");

    for (int i = 0; i < count; i++) {
        nano_print_int(procs[i].pid);
        nano_print("    ");

        nano_print_int(procs[i].ppid);
        nano_print("    ");

        // Obținem numele utilizatorului prin Syscall (Ring 3 -> Ring 0)
        char uname[32];
        nano_get_username(procs[i].uid, uname, sizeof(uname));
        
        nano_print(uname);

        // Padding pentru alinierea tabelului
        int len = (int)strlen(uname);
        for (int p = 0; p < (11 - len); p++) {
            nano_print(" ");
        }

        // Starea procesului
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