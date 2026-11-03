#include "nano_libc.h"
#include "nano_string.h"

int main(int argc, char** argv) {
    int print_newline = 1;
    int start_index = 1;

    if (argc > 1 && strcmp(argv[1], "-n") == 0) {
        print_newline = 0;
        start_index = 2;
    }

    char active_quote = 0;

    for (int i = start_index; i < argc; i++) {
        const char* arg = argv[i];
        int escaped = 0;

        for (int j = 0; arg[j] != '\0'; j++) {
            char c = arg[j];

            // 1. Dacă caracterul anterior a fost '\', tipărim caracterul curent ca text literal
            if (escaped) {
                char s[2] = {c, '\0'};
                nano_print(s);
                escaped = 0;
                continue;
            }

            // 2. Detectăm secvențele de escape (\", \', \\)
            if (c == '\\') {
                if (arg[j + 1] == '"' || arg[j + 1] == '\'' || arg[j + 1] == '\\') {
                    escaped = 1; // Sărim peste '\' și activăm modul escape pentru caracterul următor
                    continue;
                }
            }

            // 3. Procesăm ghilimelele normale (fără escape)
            if (active_quote == 0) {
                if (c == '"' || c == '\'') {
                    active_quote = c;
                } else {
                    char s[2] = {c, '\0'};
                    nano_print(s);
                }
            } else {
                if (c == active_quote) {
                    active_quote = 0;
                } else {
                    char s[2] = {c, '\0'};
                    nano_print(s);
                }
            }
        }

        if (i < argc - 1) {
            nano_print(" ");
        }
    }

    if (print_newline) {
        nano_print("\n");
    }

    return 0;
}