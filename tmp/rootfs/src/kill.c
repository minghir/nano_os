#include "nano_libc.h"

int main(int argc, char** argv) {
    if (argc != 2 || !argv || !argv[1] || argv[1][0] == '\0') {
        nano_print("Utilizare: kill <PID>\n");
        return 1;
    }

    uint32_t parsed_pid = 0;
    for (int i = 0; argv[1][i] != '\0'; i++) {
        char digit = argv[1][i];
        if (digit < '0' || digit > '9') {
            nano_print("Eroare: PID-ul trebuie sa fie numeric.\n");
            return 1;
        }

        uint32_t value = (uint32_t)(digit - '0');
        if (parsed_pid > (0x7FFFFFFFU - value) / 10) {
            nano_print("Eroare: PID invalid.\n");
            return 1;
        }
        parsed_pid = parsed_pid * 10 + value;
    }

    int target_pid = (int)parsed_pid;

    if (target_pid == 0) {
        nano_print("Eroare: Nu poti opri procesul kernel (PID 0).\n");
        return 1;
    }
	
	if (target_pid == 1) {
        nano_print("Eroare: Nu poti opri procesul de sistem init (PID 1).\n");
        return 1;
    }

    int result = nano_kill(target_pid);
    if (result == 1) {
        nano_print("Procesul a fost oprit cu succes.\n");
    } else if (result == -1) {
        nano_print("Eroare: Nu poti opri procesul curent.\n");
    } else {
        nano_print("Eroare: Procesul cu acest PID nu a fost gasit.\n");
    }

    return 0;
}