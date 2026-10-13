#include "nano_libc.h"

#define SCREEN_WIDTH  1024
#define SCREEN_HEIGHT 768

static uint32_t screen_fb[SCREEN_WIDTH * SCREEN_HEIGHT];

int main(int argc, char** argv) {
    nano_print("Pornire mod grafic fluid...\n");
    nano_set_desktop_mode(1);
    
    MouseState mouse;
    
    while (1) {
        // 1. Preluăm instantaneu starea curentă a mouse-ului din kernel
        if (!nano_get_mouse_state(&mouse)) {
            continue; // Dacă dă eroare, reîncercăm
        }

        // Ieșire instantanee dacă se apasă click dreapta sau tasta 'q'
        if (mouse.buttons & 2) break; 

        // 2. Setăm culoarea fundalului (Devine roșu la click stânga)
        uint32_t bg_color = 0xFF1A2B3C; // Albastru închis
        if (mouse.buttons & 1) {
            bg_color = 0xFFFF0000; 
        }

        for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
            screen_fb[i] = bg_color;
        }

        // 3. Desenăm un cursor alb pătrat de 12x12 pixeli pe coordonatele X, Y ale mouse-ului
        int cursor_size = 12;
        for (int dy = 0; dy < cursor_size; dy++) {
            for (int dx = 0; dx < cursor_size; dx++) {
                int px = mouse.x + dx;
                int py = mouse.y + dy;
                if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
                    screen_fb[py * SCREEN_WIDTH + px] = 0xFFFFFFFF;
                }
            }
        }

        // 4. Trimitem frame-ul direct pe ecran prin syscall-ul grafic
        nano_gfx_present(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, screen_fb);

        // 5. Verificare neblocantă pentru tasta 'q'
        if (nano_has_char()) {
            if (nano_read_char() == 'q') break;
        }
		
    }

    nano_set_desktop_mode(0);
    nano_print("Mod grafic închis.\n");
    return 0;
}