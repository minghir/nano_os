#include "nano_libc.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        nano_print("Utilizare: touch <nume_fisier>\n");
        return 0;
    }

    char* filename = argv[1];

    // Creăm un fișier cu dimensiunea inițială de 512 octeți (sau 0, în funcție de cum e scris FS-ul tău)
    int success = nano_create_file(filename, 512); 

    if (!success) {
        nano_print("touch: nu se poate crea fisierul '");
        nano_print(filename);
        nano_print("'.\n");
    }

    return 0;
}