#include "nano_libc.h"

int main(int argc, char** argv) {
    uint32_t target_uid = 0; // Default la root
    
    if (argc >= 2) {
        if (strncmp(argv[1], "user", 5) == 0) {
            target_uid = 1000;
        } else if (strncmp(argv[1], "root", 5) == 0) {
            target_uid = 0;
        } else {
            nano_print("Utilizator necunoscut. Foloseste: su user sau su root\n");
            nano_sys_exit(1);
        }
    } else {
        target_uid = 0; // 'su' simplu duce la root
    }

    // 1. Schimbăm UID-ul procesului curent
    int res = nano_setuid(target_uid);
    if (res < 0) {
        nano_print("Eroare: Permisiune refuzata (EPERM)\n");
        nano_sys_exit(1);
    }
    
    nano_print("Identitate schimbata. Se lanseaza noul shell...\n");

    // 2. Lansăm un nou shell care va moșteni noul UID (ex: 1000)
    // Asigură-te că calea către binarul tău de shell pe disc este corectă (ex: "/sbin/shell")
    int exec_res = nano_exec("/sbin/shell");
    
    // Dacă nano_exec se întoarce, înseamnă că a eșuat
    if (exec_res < 0) {
        nano_print("Eroare la pornirea subshell-ului!\n");
        nano_sys_exit(1);
    }

    nano_sys_exit(0);
}