#include "nano_libc.h"

int main(int argc, char* argv[]) {
    char dirname[128];

    // Dacă primim argument în linie (dacă shell-ul suportă), sau interactiv
    if (argc > 1 && argv[1] != 0) {
        int i = 0;
        while (argv[1][i] != '\0' && i < 127) {
            dirname[i] = argv[1][i];
            i++;
        }
        dirname[i] = '\0';
    } else {
        nano_print("Introdu numele directorului de creat: ");
        nano_readline(dirname, 127);
    }

    if (dirname[0] == '\0') {
        nano_print("Nume de director invalid.\n");
        return 0;
    }

    // Apelăm syscall-ul de mkdir
    if (nano_mkdir(dirname)) {
        nano_print("Director creat cu succes: ");
        nano_print(dirname);
        nano_print("\n");
    } else {
        nano_print("Eroare: Directorul exista deja sau discul este plin.\n");
    }

    return 0;
}