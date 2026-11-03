// audio/ac97.h
#ifndef AC97_H
#define AC97_H
#include "../audio.h"

typedef struct {
    uint32_t buffer_address; // Adresa fizică a bufferului audio în RAM
    uint16_t samples;        // Numărul de eșantioane / 2
    uint16_t flags_loops;    // Flag-uri (ex: IOC)
} __attribute__((packed)) ac97_bd_t;

AudioDriver* ac97_get_driver(void);

#endif