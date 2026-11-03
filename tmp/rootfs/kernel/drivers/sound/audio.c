// audio/audio.c
#include "audio.h"
#include "../../io.h"

static AudioDriver* current_audio_driver = 0;

void audio_register_driver(AudioDriver* driver) {
    current_audio_driver = driver;
}

void init_sound(void) {
    if (current_audio_driver && current_audio_driver->init) {
        // Poți prelua porturile extrase din PCI anterior
        current_audio_driver->init(0, 0); 
    } else {
        print("[Audio] Niciun driver audio înregistrat!\n");
    }
}

void play_pcm(const uint8_t* data, uint32_t length) {
    if (current_audio_driver && current_audio_driver->play_pcm) {
        current_audio_driver->play_pcm(data, length);
    }
}