#include "nano_libc.h"
#include "libgfx.h"

#define BOX_SIZE 100
#define SCREEN_W 1024 // Modifică dacă QEMU folosește altă rezoluție
#define SCREEN_H 768

int main(int argc, char* argv[]) {
    nano_print("\nPornesc modul grafic cu libgfx...\n");
    
    // Dacă ai un syscall să oprești textul pe fundal, apelează-l aici
    // nano_desktop_mode(1);

    // 1. Creăm un Canvas de dimensiunea întregului ecran
    GfxCanvas* main_canvas = gfx_create_canvas(SCREEN_W, SCREEN_H);
    if (!main_canvas) {
        nano_print("Eroare: Nu am putut aloca memoria pentru canvas!\n");
        return 1;
    }

    // Parametrii pătratului
    int x = 100;
    int y = 100;
    int dx = 5; // Viteza pe X
    int dy = 5; // Viteza pe Y

    uint32_t last_time = nano_get_ticks();
    key_event_t ev;


	uint32_t fps_last_time = nano_get_ticks();
    int frames_this_second = 0;
    char fps_text[32] = "FPS: 0";
    // ----------------------------
	
    // 2. GAME LOOP (Bucla principală)
    while (1) {
        uint32_t current_time = nano_get_ticks();
        
        // 1. Limitare cadre (Decomentează pentru 60 FPS, comentează pentru MAX SPEED)
        // if (current_time - last_time >= 16) { 
            last_time = current_time;

            // --- CALCUL FPS ---
            frames_this_second++;
            if (current_time - fps_last_time >= 1000) { 
                // Aici intră în scenă magia ta!
                snprintf(fps_text, sizeof(fps_text), "FPS: %d", frames_this_second);
                
                frames_this_second = 0;
                fps_last_time = current_time;
            }

            // --- DESENARE RAM ---
            gfx_clear(main_canvas, COLOR_DARK_BG);

            x += dx; y += dy;
            if (x <= 0 || x >= SCREEN_W - BOX_SIZE) dx = -dx;
            if (y <= 0 || y >= SCREEN_H - BOX_SIZE) dy = -dy;

            gfx_draw_rect(main_canvas, x, y, BOX_SIZE, BOX_SIZE, COLOR_RED);
            
            // Textul cu FPS-ul, frumos formatat
            gfx_draw_string(main_canvas, 10, 10, fps_text, COLOR_GREEN, 0x00000000);

            // --- TRIMITERE CĂTRE ECRAN ---
            gfx_present(main_canvas, 0, 0);
        // } // Sfârșit limitare cadre

        // 2. Input
        while (nano_get_key_event(&ev)) {
            if (ev.scancode == 0x01 && ev.pressed) { 
                gfx_free_canvas(main_canvas);
                return 0; 
            }
        }
        
        //nano_yield();
    }

    return 0;
}