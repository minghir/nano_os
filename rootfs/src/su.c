#include "nano_libc.h"

int main(int argc, char** argv) {
    const char* target_username = "nano"; // Implicit 'su' schimbă în root/nano
    if (argc >= 2) {
        target_username = argv[1];
    }

    uint32_t target_uid = 0;
    char home_dir[64] = {0};

    // 1. Întâi cerem Kernel-ului UID-ul și Home-ul utilizatorului
    if (nano_get_user_info(target_username, &target_uid, home_dir) < 0) {
        nano_print("su: utilizatorul '");
        nano_print(target_username);
        nano_print("' nu exista.\n");
        nano_sys_exit(1);
    }

    // 2. Schimbăm UID-ul procesului curent în Kernel
    int res = nano_setuid(target_uid);
    if (res < 0) {
        nano_print("su: Permisiune refuzata (EPERM)\n");
        nano_sys_exit(1);
    }

    // 3. Schimbăm CWD la Home Directory al noului utilizator
    if (home_dir[0] != '\0') {
        nano_cd(home_dir);
    }

    nano_print("Autentificat ca '");
    nano_print(target_username);
    nano_print("'. Se lanseaza subshell-ul...\n");

    // 4. ACUM LANSĂM SHELL-UL (după ce UID-ul și CWD-ul sunt setate corect)
    int child_pid = nano_exec("/sbin/shell");
    if (child_pid <= 0) {
        // Fallback la calea completă cu mount point dacă VFS-ul o cere
        child_pid = nano_exec("/hda/sbin/shell");
    }

    if (child_pid <= 0) {
        nano_print("su: Eroare la executarea subshell-ului!\n");
        nano_sys_exit(1);
    }

    // 5. Așteptăm ca subshell-ul să se termine (prin comanda 'exit')
    int child_exit_code = nano_wait();

    // 6. Încheiem corect sesiunea 'su' cu codul de ieșire al shell-ului
    nano_sys_exit(child_exit_code);
}