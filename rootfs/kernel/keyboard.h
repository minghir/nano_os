#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

// Structura unui eveniment de tastatură
typedef struct {
    uint8_t scancode;
    uint8_t pressed; // 1 = Apăsat, 0 = Eliberat
} key_event_t;

#define KBD_BUFFER_SIZE 256

// Funcție internă de kernel pentru a adăuga un eveniment
void kbd_push_event(uint8_t scancode, uint8_t pressed);

// Funcție pentru Syscall (extrage un eveniment)
// Returnează 1 dacă a găsit ceva, 0 dacă e gol
int kbd_pop_event(key_event_t* event_out);

#endif