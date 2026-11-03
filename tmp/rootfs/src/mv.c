#include "nano_libc.h" // headerul tău de user-space

#define BUFFER_SIZE 4096

int main(int argc, char** argv) {
    if (argc < 3) {
        nano_print("Utilizare: mv <sursa> <destinatie>\n");
        return 1;
    }

    char* src = argv[1];
    char* dst = argv[2];

    // 1. Citim fișierul sursă
    static char buffer[BUFFER_SIZE];
    int bytes_read = nano_read_file(src, buffer, BUFFER_SIZE);
    
    if (bytes_read < 0) {
        nano_print("mv: nu s-a putut citi fișierul sursă\n");
        return 1;
    }

    // 2. Creăm fișierul destinație
    if (!nano_create_file(dst, bytes_read)) {
        nano_print("mv: nu s-a putut crea destinația\n");
        return 1;
    }

    // 3. Scriem conținutul în destinație
    if (!nano_write_file(dst, buffer, bytes_read)) {
        nano_print("mv: eroare la scriere\n");
        return 1;
    }

    // 4. Dacă copierea a reușit, ștergem fișierul vechi (sursa)
    // (Presupunând că ai o funcție nano_delete_file sau nano_remove în nano_libc)
    if (!nano_delete_file(src)) {
        nano_print("mv: fișierul a fost copiat, dar sursa nu a putut fi ștearsă\n");
        return 1;
    }

    nano_print("Mutat cu succes.\n");
    return 0;
}