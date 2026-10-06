#include "nano_libc.h"

int main(int argc, char* argv[]) {
    nano_print("Inaintea lui fork()...\n");
    
    int pid = fork();
    
    if (pid < 0) {
        nano_print("Eroare la fork!\n");
        nano_sys_exit(1);
    } else if (pid == 0) {
        // Suntem în procesul COPIL
        nano_print("-> [COPIL] Mi-am facut treaba.\n");
        while(1){
		}
        // FOLOSEȘTE EXIT AICI ÎN LOC DE RETURN!
        nano_sys_exit(0); 
    } else {
        // Suntem în procesul PĂRINTE
        nano_print("-> [PARINTE] Continui sa lucrez...\n");
        
        // FOLOSEȘTE EXIT ȘI AICI!
        nano_sys_exit(0); 
    }
}
