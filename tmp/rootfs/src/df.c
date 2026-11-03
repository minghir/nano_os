#include "nano_libc.h"

// Funcție mică ajutătoare pentru a afișa un număr urmat de un număr fix de spații (padding)
void print_padded_int(uint32_t val, int target_width) {
    char buf[32];
    simple_itoa(val, buf);
    nano_print(buf);
    
    // Calculăm câte spații mai trebuie până la lățimea dorită
    int len = 0;
    while (buf[len]) len++;
    for (int i = len; i < target_width; i++) {
        nano_print(" ");
    }
}

int main(int argc, char** argv) {
    char target_path[128] = "/hda"; // Implicit
    
    if (argc > 1) {
        if (argv[1][0] != '/') {
            target_path[0] = '/';
            int i = 0;
            while (argv[1][i] && i < 120) {
                target_path[1 + i] = argv[1][i];
                i++;
            }
            target_path[1 + i] = '\0';
        } else {
            int i = 0;
            while (argv[1][i] && i < 125) {
                target_path[i] = argv[1][i];
                i++;
            }
            target_path[i] = '\0';
        }
    }

    DiskStats stats;
    if (nano_get_fs_stats(target_path, &stats) < 0) {
        nano_print("df: nu s-au putut obține statistici pentru: ");
        nano_print(target_path);
        nano_print("\n");
        return 1;
    }

    uint32_t total_kb = (stats.total_sectors * stats.sector_size) / 1024;
    uint32_t free_kb  = (stats.free_sectors * stats.sector_size) / 1024;
    uint32_t used_kb  = total_kb - free_kb;

    // Afișăm antetul aliniat cu coloanele
    nano_print("\n");
    nano_print("FILESYSTEM      SIZE (KB)    USED (KB)    AVAIL (KB)\n");
    nano_print("----------------------------------------------------\n");

    // Afișăm calea și completăm cu spații până la 16 caractere
    nano_print(target_path);
    int len = 0;
    while (target_path[len]) len++;
    for (int i = len; i < 16; i++) {
        nano_print(" ");
    }

    // Afișăm fiecare număr cu lățime fixă de aliniere (9 caractere + spații între coloane)
    print_padded_int(total_kb, 13);
    print_padded_int(used_kb,  13);
    print_padded_int(free_kb,  9);
    nano_print("\n");

    return 0;
}