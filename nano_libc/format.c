#include "nano_libc.h"

int main() {
    nano_print("=== Formatare Disc Nano OS ===\n");
    nano_print("Atentie: Toate datele vor fi sterse!\n");
    
    // Apelăm syscall-ul de formatare
    nano_format();
    
    nano_print("Discul a fost reformatat cu succes (Sistem NAN2).\n");
    return 0;
}