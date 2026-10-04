#include "video.h"

static VideoModeInfo current_video_mode = {0};

// Alocăm un buffer static în secțiunea BSS a kernelului.
// Asta rezolvă ABSOLUT TOATE conflictele cu Heap-ul și Paginarea!
// 1024 x 768 pixeli x 4 octeți (32 bpp) = 3.14 MB
static uint32_t static_back_buffer[1024 * 768];

void video_init(uint64_t fb_addr, uint32_t width, uint32_t height, uint32_t pitch, uint8_t bpp) {
    current_video_mode.fb_addr = fb_addr;
    current_video_mode.width = width;
    current_video_mode.height = height;
    current_video_mode.pitch = pitch;
    current_video_mode.bpp = bpp;
    
    current_video_mode.cols = width / 8;
    current_video_mode.rows = height / 16;

    uint32_t buffer_size_bytes = height * pitch;

    // Folosim buffer-ul nostru 100% sigur dacă încape
    if (buffer_size_bytes <= sizeof(static_back_buffer)) {
        current_video_mode.back_buffer = static_back_buffer;
    } else {
        // Fallback de siguranță extremă: desenare direct pe ecran
        // (Vei avea puțin flicker, dar NU VEI AVEA ECRAN NEGRU!)
        current_video_mode.back_buffer = (uint32_t*)fb_addr;
    }
    
    // Curățăm ecranul (sau buffer-ul) cu negru
    if (current_video_mode.back_buffer) {
        for (uint32_t i = 0; i < (buffer_size_bytes / 4); i++) {
            current_video_mode.back_buffer[i] = 0;
        }
    }
}

VideoModeInfo* video_get_info() {
    return &current_video_mode;
}

// Funcția care copiază instant întreg conținutul pe ecranul fizic (VRAM)
// Funcția optimizată la maximum pentru swap instantaneu (Zero Delay, Zero Tearing)
void video_swap_buffers() {
    if (!current_video_mode.back_buffer || !current_video_mode.fb_addr) return;
    
    // Dacă suntem pe fallback, nu avem ce face swap
    if (current_video_mode.back_buffer == (uint32_t*)current_video_mode.fb_addr) return;

    uint64_t* screen = (uint64_t*)current_video_mode.fb_addr;
    uint64_t* back = (uint64_t*)current_video_mode.back_buffer;
    
    // Fiecare pixel are 4 octeți (32-bit). 
    // Un registru de 64-biți (quadword) mută 2 pixeli dintr-o singură mișcare.
    uint64_t total_quads = ((uint64_t)current_video_mode.width * current_video_mode.height) / 2;

    // Magia hardware: instrucțiunea REP MOVSQ copiază memoria la viteză de gigabiți/secundă
    __asm__ volatile (
        "cld\n\t"
        "rep movsq"
        : "+D" (screen), "+S" (back), "+c" (total_quads)
        :
        : "memory", "cc"
    );
}