#include "nano_libc.h"

// Funcție ajutătoare pentru a parsa string-uri în bază 8 (ex: "755" -> 0755)
uint16_t parse_octal(const char* str) {
    uint16_t result = 0;
    while (*str) {
        if (*str >= '0' && *str <= '7') {
            result = (result << 3) + (*str - '0');
        } else {
            return 0xFFFF; // Caracter invalid
        }
        str++;
    }
    return result;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        nano_print("Utilizare: chmod <mod_octal> <fisier>\n");
        nano_print("Exemplu:   chmod 755 /hda/script.sh\n");
        return 1;
    }

    uint16_t mode = parse_octal(argv[1]);
    if (mode == 0xFFFF) {
        nano_print("chmod: mod octal invalid. Folositi doar cifre 0-7.\n");
        return 1;
    }

    int res = nano_chmod(argv[2], mode);
    if (res == 0) {
        nano_print("chmod: Eroare! Fisier inexistent sau acces refuzat.\n");
        return 1;
    }

    return 0;
}