#include "nano_libc.h"

int main(int argc, char** argv) {
    //nano_print("KILL START - argc: ");
    nano_print_int(argc); // Să vedem ce valoare primește efectiv!
    nano_print("\n");

    if (argc < 2) {
        nano_print("Utilizare: kill <PID>\n");
        return 1;
    }

    int target_pid = atoi(argv[1]);

    if (target_pid == 0) {
        nano_print("Eroare: Nu poti opri procesul kernel (PID 0).\n");
        return 1;
    }
	
	if (target_pid == 1) {
        nano_print("Eroare: Nu poti opri procesul de sistem init (PID 1).\n");
        return 1;
    }

    int success = nano_kill(target_pid);
    if (success) {
        nano_print("Procesul a fost oprit cu succes.\n");
    } else {
        nano_print("Eroare: Procesul cu acest PID nu a fost gasit.\n");
    }

    return 0;
}