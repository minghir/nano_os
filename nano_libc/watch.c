#include "nano_libc.h"

// Funcție simplă pentru a transforma un string în număr întreg (dacă nu o ai deja în libc)
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
        nano_print("Utilizare: watch [-n secunde] <comanda>\n");
        return 1;
    }

    int interval_ms = 1000; // Implicit: 1 secundă (1000 ms)
    int cmd_index = 1;

    // Verificăm dacă utilizatorul a folosit opțiunea -n (ex: watch -n 2 free)
    // Notă: string_compare verifică egalitatea a două string-uri
    if (argc >= 4 && strcmp(argv[1], "-n") == 0) {
        int secunde = nano_atoi(argv[2]);
        if (secunde > 0) {
            interval_ms = secunde * 1000;
        }
        cmd_index = 3; // Comanda începe după argumentul -n și valoarea lui
    }

    char* command = argv[cmd_index];

    while (1) {
        // 1. Curățăm ecranul și ducem cursorul sus (Home) folosind ANSI Escape Sequences:
        // \033[H mută cursorul la rândul 1, col 1
        // \033[J șterge tot ecranul de la cursor în jos
        nano_print("\033[H\033[J");

        // 2. Afișăm un mic antet informativ
        nano_print("Every ");
        nano_print_int(interval_ms / 1000);
        nano_print("s: ");
        nano_print(command);
        nano_print("\n----------------------------------------\n");

        // 3. Lansăm comanda cerută folosind funcțiile tale de procese
        int success = nano_exec(command);
        if (success) {
            // Așteptăm ca sub-procesul să termine execuția (prin nano_wait)
            nano_wait();
        } else {
            nano_print("watch: comanda nu a putut fi gasită sau executata: ");
            nano_print(command);
            nano_print("\n");
            break;
        }

        // 4. Pauză înainte de ciclul următor (folosind funcția ta de sleep în milisecunde)
        sleep(interval_ms);
    }

    return 0;
}