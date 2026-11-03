#include "nano_libc.h"

// Funcție mică helper pentru a transforma un număr în text și a-l afișa (dacă nu ai printf complet)
// Sau poți folosi funcțiile tale existente de afișare de numere.

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    
    MemInfo info;
    if (!nano_get_meminfo(&info)) {
        nano_print("Eroare la preluarea informațiilor despre memorie.\n");
        return 1;
    }

    nano_print("--- Nano OS Memory Information ---\n");
    
    nano_print("Heap Total : ");
    nano_print_int(info.heap_total);
    nano_print(" bytes\n");

    nano_print("Heap Folosit: ");
    nano_print_int(info.heap_used);
    nano_print(" bytes\n");

    nano_print("Heap Liber  : ");
    nano_print_int(info.heap_free);
    nano_print(" bytes\n");

    return 0;
}