#include "nano_libc.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        nano_print("Utilizare: chown <uid> <fisier>\n");
        return 1;
    }

    // Extragem NUMĂRUL direct, ocolind funcția cu bug-uri din libc!
    uint32_t target_uid = atoi(argv[1]);
    const char* filepath = argv[2];

    int res = nano_chown(filepath, target_uid, 0xFFFFFFFF);
    
    if (res == 0) {
        nano_print("chown: Eroare! Fisier inexistent sau acces refuzat (EPERM).\n");
        return 1;
    }

    return 0;
}