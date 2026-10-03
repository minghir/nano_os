#define MINIMP3_IMPLEMENTATION
#include "../thirdparty/minimp3/minimp3.h"
#include "nano_libc.h"

extern void print(const char* s);
extern uint32_t read_file_to_buffer(const char* path, uint8_t** out_buffer);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        nano_print("Utilizare: mp3play <fisier.mp3>\n");
        return 1;
    }

    // 1. Citim fișierul MP3 de pe disc
    uint8_t* mp3_data = 0;
    uint32_t mp3_size = read_file_to_buffer(argv[1], &mp3_data);
    if (!mp3_size || !mp3_data) {
        nano_print("Eroare: Nu s-a putut citi fisierul MP3!\n");
        return 1;
    }

    // 2. Inițializăm decodorul minimp3
    mp3dec_t mp3;
    mp3dec_init(&mp3);

    int16_t pcm_buffer[MINIMP3_MAX_SAMPLES_PER_FRAME];
    mp3dec_frame_info_t info;

    nano_print("Redare audio în curs...\n");

    // 3. Bucla de decodare cadru cu cadru
    uint32_t offset = 0;
    while (offset < mp3_size) {
        // Corectat: mp3dec_decode_frame în loc de mp3dec_decode
        int samples_decoded = mp3dec_decode_frame(&mp3, mp3_data + offset, mp3_size - offset, pcm_buffer, &info);
        
        if (samples_decoded > 0) {
            // Calculăm octeții PCM (eșantioane * canale * 2 octeți pentru 16-bit)
            uint32_t byte_count = samples_decoded * info.channels * sizeof(int16_t);
            
            // Trimitem prin system call către kernel și placa AC'97
            sys_play_audio((const uint8_t*)pcm_buffer, byte_count);
            
            // Avansăm offset-ul cu numărul de octeți consumați din cadrul MP3
            offset += info.frame_bytes;
        } else {
            // Dacă frame-ul nu a putut fi decodat, avansăm cu 1 octet (căutare sync word)
            offset++;
        }
    }

    nano_print("Redare terminată.\n");
    return 0;
}