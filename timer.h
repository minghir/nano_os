#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// Inițializează PIT-ul la frecvența dorită (ex: 100 Hz)
void timer_init(uint32_t frequency);

// Handler-ul apelat de rutina din assembly (poate fi marcat extern dacă e nevoie)
void timer_irq(void);

#endif