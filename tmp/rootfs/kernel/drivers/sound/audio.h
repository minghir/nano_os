#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

// "Interfața" în stil C (echivalentul unui abstract class / vtable din C++)
typedef struct {
    const char* name;
    void (*init)(uint16_t base_io, uint16_t bm_io);
    void (*play_pcm)(const uint8_t* data, uint32_t length);
    void (*stop)(void);
} AudioDriver;

// Funcții publice globale ale subsistemului audio
void audio_register_driver(AudioDriver* driver);
void init_sound(void);
void play_pcm(const uint8_t* data, uint32_t length);

#endif