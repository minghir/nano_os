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

    while (nano_get_key_event(&ev)) {
        static const unsigned char scancode_to_key[128] = {
            [0x01] = KEY_ESCAPE,
            [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
            [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
            [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
            [0x0E] = KEY_BACKSPACE, [0x0F] = KEY_TAB,
            [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
            [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
            [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
            [0x1C] = KEY_ENTER, [0x1D] = KEY_FIRE,
            [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
            [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
            [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
            [0x2A] = KEY_RSHIFT, [0x2B] = '\\',
            [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
            [0x30] = 'b', [0x31] = 'n', [0x32] = 'm',
            [0x33] = ',', [0x34] = '.', [0x35] = '/',
            [0x38] = KEY_USE, [0x39] = KEY_USE,
            [0x3B] = KEY_F1, [0x3C] = KEY_F2, [0x3D] = KEY_F3,
            [0x3E] = KEY_F4, [0x3F] = KEY_F5, [0x40] = KEY_F6,
            [0x41] = KEY_F7, [0x42] = KEY_F8, [0x43] = KEY_F9,
            [0x44] = KEY_F10,
            [0x48] = KEY_UPARROW, [0x4B] = KEY_LEFTARROW,
            [0x4D] = KEY_RIGHTARROW, [0x50] = KEY_DOWNARROW
        };
        unsigned char key = ev.scancode < sizeof(scancode_to_key)
                          ? scancode_to_key[ev.scancode] : 0;

        if (key != 0) {
            *pressed = ev.pressed;
            *doomKey = key;
            return 1;
        }
    }

    return 0;
}
