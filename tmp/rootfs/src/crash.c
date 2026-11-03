/*
#include "nano_libc.h"

int main(int argc, char* argv[]) {
    nano_print("[TEST] Programul de test a pornit.\n");
    nano_print("[TEST] Incerc sa accesez o adresa de memorie interzisa (0x0)...\n");

    // Incercăm să citim dintr-un pointer nul (Null Pointer Dereference)
    // Acest lucru va genera instant un Page Fault în hardware!
    volatile int* invalid_ptr = (volatile int*)0x0;
    int val = *invalid_ptr; 

    // Linia de mai jos nu ar trebui să se execute niciodată
    nano_print("[TEST] EROARE: Programul a trecut de crash?! Valoarea citita: ");
    nano_print_int(val); // Dacă ai funcția
    nano_print("\n");

    return 0;
}
*/

#include "nano_libc.h"

int main(int argc, char* argv[]) {
    nano_print("[TEST] Programul de test a pornit.\n");
    nano_print("[TEST] Incerc sa accesez o adresa nerealizata (0x50000000)...\n");

    // Adresa 0x50000000 (1.25 GB) este mult peste cei 32 MB mapați în Identity Mapping
    volatile int* invalid_ptr = (volatile int*)0x50000000;
    int val = *invalid_ptr; // Aici se va produce Page Fault-ul real!

    nano_print("[TEST] EROARE: Programul a trecut de crash?! Valoarea: ");
    nano_print("\n");

    return 0;
}