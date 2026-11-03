#ifndef SPEAKER_H
#define SPEAKER_H

#include <stdint.h>
#include "io.h" // Aici ai funcțiile tale outb și inb

// Pornește sunetul la o anumită frecvență (ex: 440 Hz)
void speaker_play(uint32_t frequency);

// Oprește sunetul
void speaker_stop();

// Funcție utilitară: emite un beep la o frecvență și o durată anume (în milisecunde)
void beep(uint32_t frequency, uint32_t duration_ms);

#endif