#include "nano_libc.h" // headerul tău de user-space (cu nano_read, nano_write, etc.)

#define BUFFER_SIZE 4096

int main(int argc, char** argv) {
    if (argc < 3) {
        nano_print("Utilizare: cp <sursa> <destinatie>\n");
        return 1;
    }

    char* src = argv[1];
    char* dst = argv[2];

    // 1. Deschidem / citim fișierul sursă prin user-space API
    static char buffer[BUFFER_SIZE];
    int bytes_read = nano_read_file(src, buffer, BUFFER_SIZE);
    
    if (bytes_read < 0) {
        nano_print("cp: nu s-a putut citi fișierul sursă\n");
        return 1;
    }

    // 2. Creăm și scriem fișierul destinație
    if (!nano_create_file(dst, bytes_read)) {
        nano_print("cp: nu s-u putut crea destinația\n");
        return 1;
    }

    if (!nano_write_file(dst, buffer, bytes_read)) {
        nano_print("cp: eroare la scriere\n");
        return 1;
    }

    nano_print("Copiat cu succes.\n");
    return 0;
}