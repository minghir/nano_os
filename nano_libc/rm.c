#include "nano_libc.h"

int main() {
    nano_print("=== Stergere Fisier (rm) ===\n");
    nano_print("Introdu numele fisierului de sters: ");

    char filename[64];
    nano_readline(filename, 63);

    if (filename[0] == '\0') {
        nano_print("Nume invalid.\n");
        return 0;
    }

    nano_print("Se sterge: ");
    nano_print(filename);
    nano_print("\n");

    if (nano_delete_file(filename)) {
        nano_print("Fisier sters cu succes!\n");
    } else {
        nano_print("Eroare: Fisierul nu a fost gasit sau nu poate fi sters.\n");
    }

    return 0;
}