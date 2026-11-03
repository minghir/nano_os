#include "nano_libc.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        nano_print("Utilizare: wavplay <fisier.wav>\n");
        return 1;
    }

    // 1. Alocăm un buffer în heap-ul procesului (ex: 64 KB, compatibil cu limitele paginilor)
    uint32_t max_size = 64 * 1024;
    uint8_t* buf = (uint8_t*)nano_malloc(max_size);
    if (!buf) {
        nano_print("Eroare: Memorie insuficientă în heap!\n");
        return 1;
    }

    // 2. Citim fișierul WAV de pe disc folosind syscall-ul existent
    int bytes_read = nano_read_file(argv[1], buf, max_size);
    if (bytes_read <= 44) {
        nano_print("Eroare: Fișier WAV invalid sau prea mic!\n");
        nano_free(buf);
        return 1;
    }

    // 3. Validare simplă a antetului RIFF/WAVE (primele 4 octeți "RIFF", iar la offset 8 "WAVE")
    if (buf[0] != 'R' || buf[1] != 'I' || buf[2] != 'F' || buf[3] != 'F' ||
        buf[8] != 'W' || buf[9] != 'A' || buf[10] != 'V' || buf[11] != 'E') {
        nano_print("Eroare: Fișierul nu are formatul valid WAV (RIFF/WAVE)!\n");
        nano_free(buf);
        return 1;
    }

    nano_print("Redare WAV în curs...\n");

    // 4. Datele PCM brute încep după antetul standard de 44 de octeți
    const uint8_t* pcm_data = buf + 44;
    uint32_t pcm_size = (uint32_t)bytes_read - 44;

    // 5. Trimitem datele PCM către kernel în bucăți (de 4096 octeți, 
    // potrivite cu dimensiunea bufferului DMA configurat în driverul AC'97)
    uint32_t chunk_size = 4096;
    uint32_t offset = 0;

    while (offset < pcm_size) {
        uint32_t remaining = pcm_size - offset;
        uint32_t current_chunk = (remaining < chunk_size) ? remaining : chunk_size;

        // Apelăm system call-ul de audio (syscall 36)
        sys_play_audio(pcm_data + offset, current_chunk);

        offset += current_chunk;
    }

    nano_print("Redare WAV terminată.\n");
    nano_free(buf);
    return 0;
}