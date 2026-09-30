#include "nano_libc.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        nano_print("Utilizare: rm <nume_fisier>\n");
        return 0;
    }

    char* filename = argv[1];

    if (nano_delete_file(filename)) {
        // Opțional: nu afișăm nimic la succes (modul "silențios" din Linux) 
        // sau lăsăm un mic mesaj pentru feedback:
        // nano_print("Fisier sters.\n");
    } else {
        nano_print("rm: nu se poate sterge '");
        nano_print(filename);
        nano_print("': Fisier inexistent sau eroare de sistem.\n");
    }

    return 0;
}