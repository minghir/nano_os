#include "nano_libc.h"

// Definim rezoluția completă a ecranului
#define SCREEN_WIDTH  1024
#define SCREEN_HEIGHT 768

// Buffer-ul pentru tot ecranul
static uint32_t screen_fb[SCREEN_WIDTH * SCREEN_HEIGHT];

// Desenăm un cursor clasic de mouse (săgeată)
static const char* mouse_sprite[] = {
    "X           ",
    "XX          ",
    "X.X         ",
    "X..X        ",
    "X...X       ",
    "X....X      ",
    "X.....X     ",
    "X......X    ",
    "X.......X   ",
    "X........X  ",
    "X.....XXXXX ",
    "X..X..X     ",
    "X.X X..X    ",
    "XX   X..X   ",
    "X     X..X  ",
    "       XX   "
};

void draw_mouse_cursor(int mx, int my) {
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 12; x++) {
            int py = my + y;
            int px = mx + x;
            
            // Verificăm să nu desenăm în afara ecranului
            if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
                char pixel = mouse_sprite[y][x];
                if (pixel == 'X') {
                    screen_fb[py * SCREEN_WIDTH + px] = 0xFF000000; // Contur negru
                } else if (pixel == '.') {
                    screen_fb[py * SCREEN_WIDTH + px] = 0xFFFFFFFF; // Interior alb
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    nano_print("Pornire desktop GUI...\n");
    nano_set_desktop_mode(1);
    
    MouseState mouse;
    
    // Coordonatele ferestrei noastre "roșii"
    int win_x = 400;
    int win_y = 300;
    int win_w = 150;
    int win_h = 150;

    while (1) {
        // 1. Desenăm fundalul ecranului (ex: un albastru închis ca un desktop)
        for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
            screen_fb[i] = 0xFF1A2B3C; 
        }

        // 2. Desenăm "fereastra" roșie cu chenar albastru pe desktop
        for (int y = 0; y < win_h; y++) {
            for (int x = 0; x < win_w; x++) {
                int py = win_y + y;
                int px = win_x + x;
                
                if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
                    if (y < 20 || y >= 130 || x < 20 || x >= 130) {
                        screen_fb[py * SCREEN_WIDTH + px] = 0xFFFF0000; // Roșu
                    } else {
                        screen_fb[py * SCREEN_WIDTH + px] = 0xFF0000FF; // Albastru
                    }
                }
            }
        }

        // 3. Citim poziția mouse-ului
        if (nano_get_mouse_state(&mouse)) {
            // Ieșim la Click Stânga dacă e apăsat
            if (mouse.buttons & 1) {
                break;
            }

            // 4. Desenăm cursorul în noul buffer, indiferent unde se află pe ecran!
            draw_mouse_cursor(mouse.x, mouse.y);
        }

        // 5. Trimitem întregul desktop pe ecran (0, 0, 1024, 768)
        nano_gfx_present(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, screen_fb);
        
        // Verificăm și tasta 'q' pentru ieșire
        if (nano_has_char()) {
            if (nano_read_char() == 'q') break;
        }

        nano_sleep(16); // ~60 FPS pentru mișcare extrem de fluidă
    }

    // Resetăm totul la ieșire
    nano_set_desktop_mode(0);
    nano_print("GUI inchis.\n");
    return 0;
}