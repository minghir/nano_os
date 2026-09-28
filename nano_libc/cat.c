#include "nano_libc.h"

int main() {
    nano_print("=== Vizualizare Fisier (cat) ===\n");
    nano_print("Introdu numele fisierului: ");

    char filename[128];
    nano_readline(filename, 127);

    if (filename[0] == '\0') {
        nano_print("Nume invalid.\n");
        return 0;
    }

    uint8_t buffer[1024];
    int bytes_read = nano_read_file(filename, buffer, 1023);

    if (bytes_read <= 0) {
        nano_print("Eroare: Nu s-a putut citi fisierul sau acesta nu exista.\n");
        return 0;
    }

    if (bytes_read < 1024) {
        buffer[bytes_read] = '\0';
    } else {
        buffer[1023] = '\0';
    }

    nano_print("\n--- Continut ---\n");
    nano_print((char*)buffer);
    nano_print("\n----------------\n");

    return 0;
}