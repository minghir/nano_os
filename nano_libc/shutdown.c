#include "nano_libc.h"

int main() {
    nano_print("Nano OS se inchide...\n");
    nano_shutdown();
    
    // Dacă din orice motiv nu se închide instant, oprim execuția
    while(1) {
        __asm__ volatile ("hlt");
    }
    return 0;
}