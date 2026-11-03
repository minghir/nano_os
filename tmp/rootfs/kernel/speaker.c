#include <stdint.h>

#include "timer.h"
#include "io.h" // Aici ai funcțiile tale outb și inb

// Declară o funcție de sleep/delay dacă o ai în kernel (ex: sleep(ms))
extern void sleep(uint32_t ms); 

// Pornește sunetul la o anumită frecvență (ex: 440 Hz)
void speaker_play(uint32_t frequency) {
    if (frequency == 0) return;

    // 1. Calculăm divizorul pentru PIT (Ceas de bază: 1.193.180 Hz)
    uint32_t div = 1193180 / frequency;

    // 2. Trimitem comanda către PIT (Canalul 2, Modul 3 - Square Wave Generator)
    outb(0x43, 0xB6);

    // 3. Trimitem divizorul (mai întâi Byte-ul inferior, apoi cel superior)
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));

    // 4. Activăm difuzorul prin portul 0x61 (setăm biții 0 și 1)
    uint8_t tmp = inb(0x61);
    if ((tmp & 3) != 3) {
        outb(0x61, tmp | 3);
    }
}

// Oprește sunetul
void speaker_stop() {
    uint8_t tmp = inb(0x61) & 0xFC; // Ștergem biții 0 și 1 (oprim difuzorul)
    outb(0x61, tmp);
}

// Funcție utilitară: emite un beep la o frecvență și o durată anume (în milisecunde)
// Funcția beep corectată care garantează că oprește difuzorul
void beep(uint32_t frequency, uint32_t duration_ms) {
    if (frequency == 0) return;

    // 1. Pornim sunetul
    uint32_t div = 1193180 / frequency;
    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));

    uint8_t tmp = inb(0x61);
    if ((tmp & 3) != 3) {
        outb(0x61, tmp | 3);
    }

    // 2. Așteptă perioada cerută folosind delay-ul intern sigur
    delay_ms(duration_ms);

    // 3. Oprim obligatoriu difuzorul la final!
    tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}