#include "keyboard.h"

key_event_t kbd_buffer[KBD_BUFFER_SIZE];
volatile int kbd_head = 0;
volatile int kbd_tail = 0;

// Funcție internă de kernel pentru a adăuga un eveniment
void kbd_push_event(uint8_t scancode, uint8_t pressed) {
    int next_head = (kbd_head + 1) % KBD_BUFFER_SIZE;

    if (next_head != kbd_tail) { // Dacă buffer-ul nu e plin
        kbd_buffer[kbd_head].scancode = scancode;
        kbd_buffer[kbd_head].pressed  = pressed;   // <-- AICI ERA GREȘEALA
        kbd_head = next_head;
    }
}

// Funcție pentru Syscall (extrage un eveniment)
// Returnează 1 dacă a găsit ceva, 0 dacă e gol
int kbd_pop_event(key_event_t* event_out) {
    if (kbd_head == kbd_tail) return 0; // Gol
    
    *event_out = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    return 1;
}
