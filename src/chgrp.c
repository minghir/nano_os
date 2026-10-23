#include "nano_libc.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        nano_print("Utilizare: chgrp <gid> <fisier>\n");
        nano_print("Exemplu:   chgrp 1000 /hda/fisier.txt\n");
        return 1;
    }

    // Aici presupunem GID numeric. Dacă ai o funcție nano_get_group_info în viitor, 
    // poți face la fel ca la chown.
    uint32_t target_gid = atoi(argv[1]);
    const char* filepath = argv[2];

    // Trimitem 0xFFFFFFFF (-1) pentru UID pentru a-i spune kernelului să NU schimbe userul
    int res = nano_chown(filepath, 0xFFFFFFFF, target_gid);
    
    if (res == 0) {
        nano_print("chgrp: Eroare! Fisier inexistent sau acces refuzat.\n");
        return 1;
    }

    return 0;
}