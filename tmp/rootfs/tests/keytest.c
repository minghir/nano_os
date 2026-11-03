#include "nano_libc.h"

// Harta stării tastelor (1 = apăsat, 0 = liber)
uint8_t key_state[128] = {0};

int main(int argc, char* argv[]) {
    nano_print("\n=== DOOM ENGINE INPUT TEST ===\n");
    nano_print("Tine apasat W, A, S, D.\n");
    nano_print("Apasa ESC (Scancode 1) pentru iesire.\n\n");

    key_event_t ev;
    
    int actiune_curenta = 0;
    int actiune_precedenta = -1; // -1 ca sa printeze starea initiala

    // GAME LOOP-ul principal
    while (1) {
        
        // 1. PROCESĂM TOATE EVENIMENTELE (Golim țeava)
        while (nano_get_key_event(&ev)) {
            if (ev.scancode < 128) {
                key_state[ev.scancode] = ev.pressed;
            }
        }

        // Condiție de ieșire
        if (key_state[0x01]) { // 0x01 este ESC
            nano_print("\nIesire din test...\n");
            break;
        }

        // 2. DETERMINĂM CE FACE DOOMGUY ACUM
        if (key_state[17]) actiune_curenta = 1;      // W (Fata)
        else if (key_state[31]) actiune_curenta = 2; // S (Spate)
        else if (key_state[30]) actiune_curenta = 3; // A (Stanga)
        else if (key_state[32]) actiune_curenta = 4; // D (Dreapta)
        else actiune_curenta = 0;                    // Sta pe loc

        // 3. PRINTĂM DOAR DACĂ S-A SCHIMBAT CEVA!
        if (actiune_curenta != actiune_precedenta) {
            if (actiune_curenta == 1) nano_print("Doomguy merge in FATA!\n");
            else if (actiune_curenta == 2) nano_print("Doomguy merge in SPATE!\n");
            else if (actiune_curenta == 3) nano_print("Doomguy face STRAFE STANGA!\n");
            else if (actiune_curenta == 4) nano_print("Doomguy face STRAFE DREAPTA!\n");
            else nano_print("Doomguy sta pe loc...\n");
            
            actiune_precedenta = actiune_curenta;
        }
        
        nano_yield();
    }

    return 0;
}