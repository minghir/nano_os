#include "nano_libc.h"

int main(int argc, char* argv[]) {
    // 1. Verificăm dacă utilizatorul a dat un nume de fișier ca argument
    if (argc < 2) {
        nano_print("Utilizare: cat <nume_fisier>\n");
        return 0;
    }

    // 2. Numele fișierului este primul argument (argv[1])
    char* filename = argv[1];

    uint8_t buffer[1024];
    int bytes_read = nano_read_file(filename, buffer, 1023);

    if (bytes_read <= 0) {
        nano_print("cat: '");
        nano_print(filename);
        nano_print("': Nu exista sau nu poate fi citit.\n");
        return 0;
    }

    // Asigurăm terminarea șirului de caractere
    buffer[bytes_read] = '\0';

    // 3. Afișăm conținutul direct (fără decoruri inutile, ca un utilitar real de Linux)
    nano_print((char*)buffer);
    nano_print("\n");

    return 0;
}