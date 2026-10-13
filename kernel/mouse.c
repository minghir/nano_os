#include "mouse.h"
#include "string.h"
#include "io.h"

// Starea globală a mouse-ului (accesibilă în tot kernelul)
int mouse_x = 400; // Centrul ecranului implicit
int mouse_y = 300;
uint8_t mouse_buttons = 0;
volatile int mouse_moved = 0;

// Valori implicite pentru dimensiuni (pot fi suprascrise de la video_get_info)
#define DEFAULT_SCREEN_WIDTH  1024
#define DEFAULT_SCREEN_HEIGHT 768

// Funcție apelată exclusiv de ps2_mouse.c când primește date brute de la hardware
void mouse_update_position(int dx, int dy, uint8_t buttons) {
    mouse_x += dx;
    mouse_y -= dy; // Inversat pe axa Y deoarece coordonatele ecranului cresc în jos

    // Limite ecran (lăsând un mic buffer de 8 pixeli pentru cursor)
    int max_w = DEFAULT_SCREEN_WIDTH - 8;
    int max_h = DEFAULT_SCREEN_HEIGHT - 8;

    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x > max_w) mouse_x = max_w;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y > max_h) mouse_y = max_h;

    mouse_buttons = buttons;
    mouse_moved = 1;
	
}

// Funcție pentru a prelua starea curentă (folosită de handlerul Syscall)
void mouse_get_state(MouseState* state) {
    if (!state) return;
    state->x = mouse_x;
    state->y = mouse_y;
    state->buttons = mouse_buttons;
	print("AAAA X:=");
	print_number(mouse_x);
}