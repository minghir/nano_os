// audio/ac97.c
#include "ac97.h"
#include "../../../io.h"
static uint16_t g_mixer_base = 0;
static uint16_t g_bm_base = 0;

// Alocăm un buffer static sau dinamic pentru DMA (trebuie să fie accesibil fizic)
// Pentru test, putem folosi un buffer simplu de eșantioane PCM
#define AUDIO_BUFFER_SIZE 4096
static uint8_t audio_dma_buffer[AUDIO_BUFFER_SIZE];
static ac97_bd_t bd_list[32]; // Lista de descriptori

static void ac97_init_impl(uint16_t mixer_base, uint16_t bm_base) {
    g_mixer_base = mixer_base;
    g_bm_base = bm_base;

    print("[AC97] Initializare hardware AC'97...\n");

    if (g_mixer_base == 0 || g_bm_base == 0) {
        print("[AC97] Erore: Porturi invalide (BAR0/BAR1 neconfigurate)!\n");
        return;
    }

    // 1. Resetare mixer și setare volum maxim
    outw(g_mixer_base + 0x00, 0x0000);
    outw(g_mixer_base + 0x02, 0x0000); // Master Out
    outw(g_mixer_base + 0x18, 0x0000); // PCM Out

    // 2. Resetare canal PCM Out Bus Master (offset 0x1B)
    outb(g_bm_base + 0x1B, 0x02); 
    io_wait();
    outb(g_bm_base + 0x1B, 0x01); // Setăm bitul RP (Run/Pause)

    print("[AC97] Controlerul AC'97 este pregatit si deblocat!\n");
}

static void ac97_play_pcm_impl(const uint8_t* data, uint32_t length) {
    if (g_bm_base == 0) return;

    print("[AC97] Configurare DMA și pornire redare PCM...\n");

    // Copiem datele în bufferul fizic (limitat la dimensiunea maximă)
    uint32_t copy_len = length > AUDIO_BUFFER_SIZE ? AUDIO_BUFFER_SIZE : length;
    for (uint32_t i = 0; i < copy_len; i++) {
        audio_dma_buffer[i] = data[i];
    }

    // Configurăm primul Buffer Descriptor (BD)
    bd_list[0].buffer_address = (uint32_t)(uintptr_t)audio_dma_buffer; // Adresa fizică
    bd_list[0].samples = (uint16_t)(copy_len / 2); // Numărul de eșantioane / 2 (pentru 16-bit)
    bd_list[0].flags_loops = 0x8000; // Flag-ul IOC (Interrupt On Completion)

    // Setăm adresa listei de descriptori în registrul BDBAR al canalului PCM Out (offset 0x00 în Bus Master)
    outl(g_bm_base + 0x00, (uint32_t)(uintptr_t)bd_list);

    // Setăm Last Valid Index (LVI) la 0 (avem un singur descriptor activ, offset 0x05)
    outb(g_bm_base + 0x05, 0);

    // Pornim canalul setând bitul Run/Pause (CR = 0x01) în registrul de control (offset 0x0B)
    outb(g_bm_base + 0x0B, 0x01);

    print("[AC97] Flux PCM trimis către placa de sunet!\n");
}

static void ac97_stop_impl(void) {
    if (g_bm_base != 0) {
        // Oprim canalul
        outb(g_bm_base + 0x0B, 0x00);
    }
    print("[AC97] Redarea PCM a fost oprită.\n");
}

static AudioDriver ac97_driver = {
    .name = "Intel AC'97",
    .init = ac97_init_impl,
    .play_pcm = ac97_play_pcm_impl,
    .stop = ac97_stop_impl
};

AudioDriver* ac97_get_driver(void) {
    return &ac97_driver;
}