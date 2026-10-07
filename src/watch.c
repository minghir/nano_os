#include "nano_libc.h"

// Funcție simplă pentru a transforma un string în număr întreg
int nano_atoi(const char* s) {
    int res = 0;
    while (*s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        s++;
    }
    return res;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        nano_print("Utilizare: watch [-n secunde] <comanda [argumente...]>\n");
        return 1;
    }

    int interval_ms = 1000; // Implicit: 1 secundă (1000 ms)
    int cmd_index = 1;

    // Verificăm dacă utilizatorul a folosit opțiunea -n (ex: watch -n 2 ls -l)
    if (argc >= 3 && strcmp(argv[1], "-n") == 0) {
        int secunde = nano_atoi(argv[2]);
        if (secunde > 0) {
            interval_ms = secunde * 1000;
        }
        cmd_index = 3; // Comanda începe după "-n" și valoarea lui
    }

    // Verificăm dacă a rămas vreo comandă de executat
    if (argc <= cmd_index) {
        nano_print("Utilizare: watch [-n secunde] <comanda [argumente...]>\n");
        return 1;
    }

    // Reconstruim întreaga linie de comandă cu tot cu argumentele ei într-un buffer
    char command_buf[256];
    int buf_idx = 0;
    command_buf[0] = '\0';

    for (int i = cmd_index; i < argc; i++) {
        char* arg = argv[i];
        int j = 0;
        while (arg[j] != '\0' && buf_idx < 254) {
            command_buf[buf_idx++] = arg[j++];
        }
        // Adăugăm un spațiu între argumente (dacă nu suntem la ultimul argument)
        if (i < argc - 1 && buf_idx < 254) {
            command_buf[buf_idx++] = ' ';
        }
    }
    command_buf[buf_idx] = '\0';

    while (1) {
        // 1. Curățăm ecranul și ducem cursorul sus (Home) folosind ANSI Escape Sequences
        nano_print("\033[H\033[J");

        // 2. Afișăm antetul informativ cu comanda completă
        nano_print("Every ");
        nano_print_int(interval_ms / 1000);
        nano_print("s: ");
        nano_print(command_buf);
        nano_print("\n----------------------------------------\n");

        // 3. Lansăm comanda complexă folosind funcția ta de execuție
        int success = nano_exec(command_buf);
        if (success) {
            // Așteptăm ca sub-procesul să termine execuția
            nano_wait();
        } else {
            nano_print("watch: comanda nu a putut fi gasită sau executata: ");
            nano_print(command_buf);
            nano_print("\n");
            break;
        }

        // 4. Pauză înainte de ciclul următor
        sleep(interval_ms);
    }

    return 0;
}