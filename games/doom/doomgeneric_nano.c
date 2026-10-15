#include "doomgeneric.h"
#include "doomkeys.h"
#include "nano_libc.h"
#include "libgfx.h"

// Doomgeneric randează intern jocul la rezoluția clasică de 320x200.
// Expune acest buffer global pe care noi trebuie doar să-l trimitem pe ecran.

void DG_Init() {
    nano_print("DOOM: Initializare sistem grafic...\n");
    // Dacă ai un apel pentru a ascunde textul, pune-l aici
}

void DG_DrawFrame() {
    // Trimitem cadrul randat de DOOM direct către syscall-ul nostru!
    // DG_ScreenBuffer este array-ul global în care DOOM tocmai a desenat.
    // Va apărea în colțul ecranului (320x200). Mai târziu îl putem scala!
    nano_gfx_present(0, 0, DOOMGENERIC_RESX, DOOMGENERIC_RESY, DG_ScreenBuffer);
}

void DG_SleepMs(uint32_t ms) {
    uint32_t start = nano_get_ticks();
    while (nano_get_ticks() - start < ms) {
        nano_yield(); // Nu ardem procesorul
    }
}

uint32_t DG_GetTicksMs() {
    return nano_get_ticks();
}

int DG_GetKey(int* pressed, unsigned char* doomKey) {
    key_event_t ev;
    
    // Extragem din buffer-ul nostru non-blocant!
    if (nano_get_key_event(&ev)) {
        *pressed = ev.pressed;

        // Mapăm hardware-ul Nano OS la ce știe DOOM
        switch (ev.scancode) {
            case 0x01: *doomKey = KEY_ESCAPE; break;
            case 0x1C: *doomKey = KEY_ENTER; break;
            case 0x39: *doomKey = ' '; break;             // SPACE (Deschide uși)
            case 0x1D: *doomKey = KEY_FIRE; break;        // L-CTRL (Trage)
            case 0x38: *doomKey = KEY_USE; break;         // L-ALT (Folosește)
            
            // Săgețile (în funcție de cum le trimite kernelul tău)
            case 0x48: *doomKey = KEY_UPARROW; break;
            case 0x50: *doomKey = KEY_DOWNARROW; break;
            case 0x4B: *doomKey = KEY_LEFTARROW; break;
            case 0x4D: *doomKey = KEY_RIGHTARROW; break;
            
            // Pentru W, A, S, D, Q, E și alte caractere
            default:
                // TODO: Aici va trebui o conversie fină din scancode în literă mică ASCII.
                // Momentan trimitem o valoare generică
                *doomKey = (unsigned char)ev.scancode; 
                break;
        }
        return 1; // Am găsit o tastă!
    }
    
    return 0; // Nu s-a apăsat nimic
}
