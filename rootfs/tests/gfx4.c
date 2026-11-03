#include "nano_libc.h"
#include "nano_string.h"
#include "libgfx.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    nano_set_desktop_mode(1);

    GfxCanvas* canvas = gfx_create_canvas(1024, 768);
    if (!canvas) {
        nano_print("Eroare: Nu s-a putut crea canvasul grafic!\n");
        nano_set_desktop_mode(0);
        return 1;
    }

    GfxWindowManager* wm = gfx_wm_create();
    if (!wm) {
        nano_print("Eroare: Nu s-a putut crea Window Manager-ul!\n");
        gfx_free_canvas(canvas);
        nano_set_desktop_mode(0);
        return 1;
    }

    GfxWindow win1, win2;
    gfx_init_window(&win1, 100, 100, 400, 300, "Fereastra Principala");
    gfx_init_window(&win2, 550, 150, 420, 280, "Fereastra Secundara");

    // Asigurăm fundal alb de la început
    if (win1.canvas) gfx_clear(win1.canvas, COLOR_WHITE);
    if (win2.canvas) gfx_clear(win2.canvas, COLOR_WHITE);

    gfx_wm_add_window(wm, &win1);
    gfx_wm_add_window(wm, &win2);

    MouseState mouse;
    int counter = 0;
    char buf[64];

    while (1) {
        if (!nano_get_mouse_state(&mouse)) continue;
        if (mouse.buttons & 2) break; 

        gfx_wm_handle_mouse(wm, mouse.x, mouse.y, mouse.buttons);

        // Randăm conținutul pentru Fereastra 1 din main
        if (!win1.closed && win1.canvas) {
            gfx_clear(win1.canvas, COLOR_WHITE); 
            gfx_draw_string(win1.canvas, 15, 20, "Fereastra 1 activa.", COLOR_BLACK, 0x00000000);
        }

        // Randăm conținutul pentru Fereastra 2 tot din main (pentru test)
        if (!win2.closed && win2.canvas) {
            gfx_clear(win2.canvas, COLOR_WHITE); 
            gfx_draw_string(win2.canvas, 15, 20, "Fereastra 2 (Test din Main)", COLOR_BLACK, 0x00000000);
            
            snprintf(buf, sizeof(buf), "Contor: %d", counter++);
            gfx_draw_string(win2.canvas, 15, 45, buf, GFX_RGB(0, 120, 0), 0x00000000);
        }

        gfx_clear(canvas, COLOR_DARK_BG);
        gfx_wm_draw_all(wm, canvas);
        gfx_draw_cursor(canvas, mouse.x, mouse.y);
        gfx_present(canvas, 0, 0);

        if (nano_has_char() && nano_read_char() == 'q') break;
    }

    gfx_wm_free(wm);
    gfx_free_canvas(canvas);
    nano_set_desktop_mode(0);

    return 0;
}