#include "nano_libc.h"
#include "stdio.h"
#include "string.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Utilizare: script <fisier.sh>\n");
        return 1;
    }

    const char* filename = argv[1];
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("script: nu s-a putut deschide fisierul: %s\n", filename);
        return 1;
    }

    char line[256];
    int line_num = 0;

    // Citim fişierul linie cu linie
    while (fgets(line, sizeof(line), file) != NULL) {
        line_num++;

        // Curățăm caracterele newline (\n sau \r) de la sfârșitul liniei
        int len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[len - 1] = '\0';
            len--;
        }

        // Trecem peste rândurile goale sau comentarii (care încep cu '#')
        int start = 0;
        while (line[start] == ' ' || line[start] == '\t') start++;
        if (line[start] == '\0' || line[start] == '#') {
            continue;
        }

        // Opțional: poți afișa linia care se execută (stil bash set -x)
        // printf("+ %s\n", &line[start]);

        // Executăm comanda folosind funcția ta nativă de execuție din kernel
        int child_pid = nano_exec(&line[start]);
        if (child_pid > 0) {
            nano_wait(); // Aşteptăm terminarea comenzii curente din script
        } else {
            printf("script: eroare la linia %d: %s\n", line_num, &line[start]);
        }
    }

    fclose(file);
    return 0;
}