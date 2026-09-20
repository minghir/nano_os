#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>


// Structură pentru timp
typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
} DateTime;


// Inițializează PIT-ul la frecvența dorită (ex: 100 Hz)
void timer_init(uint32_t frequency);

// Handler-ul apelat de rutina din assembly (poate fi marcat extern dacă e nevoie)
void timer_irq(void);


DateTime get_current_time();

void sleep_ms(uint32_t milliseconds);

#endif