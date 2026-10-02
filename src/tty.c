#include "tty.h"
#include "io.h"

// Alocarea memoriei pentru variabilele globale
TTY ttys[MAX_TTYS];
int active_tty = 1;

extern void cursor_update();

void switch_tty(int new_tty) {
    if (new_tty < 0 || new_tty >= MAX_TTYS || new_tty == active_tty) {
        return;
    }

    active_tty = new_tty; // Schimbăm focusul

    uint16_t* physical_vga = (uint16_t*)0xB8000;
    
    // Oglindim pe ecran memoria terminalului la care tocmai ne-am mutat
    for (int i = 0; i < 80 * 25; i++) {
        physical_vga[i] = ttys[active_tty].screen_buffer[i];
    }

    // Actualizăm hardware-ul să arate cursorul corect
    cursor_update();
}

int get_active_tty() {
    return active_tty;
}

void tty_init() {
    uint16_t* physical_vga = (uint16_t*)0xB8000;
    for (int t = 0; t < MAX_TTYS; t++) {
        ttys[t].cursor = 0;
        ttys[t].current_vga_attr = FONT_COLOR; // FONT_COLOR vine din io.h
        ttys[t].keyboard_queue_read = 0;
        ttys[t].keyboard_queue_write = 0;
        for (int i = 0; i < 80 * 25; i++) {
            ttys[t].screen_buffer[i] = FONT_COLOR | ' ';
        }
    }
    for (int i = 0; i < 80 * 25; i++) {
        physical_vga[i] = ttys[active_tty].screen_buffer[i];
    }
    cursor = 0;
    cursor_update();
}