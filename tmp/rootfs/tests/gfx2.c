#include "nano_libc.h"
#include "nano_string.h"
#include "libgfx.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    nano_set_desktop_mode(1);

    // Inițializare canvas grafic principal
    GfxCanvas* canvas = gfx_create_canvas(1024, 768);
    if (!canvas) {
        nano_print("Eroare: Nu s-a putut crea canvasul grafic!\n");
        nano_set_desktop_mode(0);
        return 1;
    }

    // Creăm Window Manager-ul integrat în libgfx
    GfxWindowManager* wm = gfx_wm_create();
    if (!wm) {
        nano_print("Eroare: Nu s-a putut crea Window Manager-ul!\n");
        gfx_free_canvas(canvas);
        nano_set_desktop_mode(0);
        return 1;
    }

    // Inițializăm două ferestre (fiecare își va aloca propriul canvas intern de conținut)
    GfxWindow win1, win2;
    gfx_init_window(&win1, 100, 100, 400, 300, "Fereastra Unu");
    gfx_init_window(&win2, 250, 180, 450, 320, "Fereastra Doi (Control Panel)");

    // Le adăugăm în manager
    gfx_wm_add_window(wm, &win1);
    gfx_wm_add_window(wm, &win2);

    MouseState mouse;

    while (1) {
        // Citim starea mouse-ului
        if (!nano_get_mouse_state(&mouse)) continue;
        if (mouse.buttons & 2) break; // Ieșire la click dreapta

        // Actualizăm starea ferestrelor și Z-order-ul prin Window Manager
        gfx_wm_handle_mouse(wm, mouse.x, mouse.y, mouse.buttons);

        // 1. Populăm conținutul intern al ferestrelor DOAR dacă sunt deschise
        if (!win1.closed && win1.canvas) {
            gfx_clear(win1.canvas, COLOR_WHITE); // Fundal alb pentru interiorul ferestrei 1
            gfx_draw_string(win1.canvas, 15, 20, "Salut din interiorul ferestrei 1!", COLOR_BLACK, 0x00000000);
            gfx_draw_string(win1.canvas, 15, 45, "Fiecare fereastra are acum canvasul ei.", COLOR_BLACK, 0x00000000);
        }

        if (!win2.closed && win2.canvas) {
            gfx_clear(win2.canvas, COLOR_WHITE); // Fundal alb pentru interiorul ferestrei 2
            gfx_draw_string(win2.canvas, 15, 20, "Panou de control Nano OS", COLOR_BLACK, 0x00000000);
            gfx_draw_string(win2.canvas, 15, 45, "Nu se mai suprapun textele pe ecran!", COLOR_BLACK, 0x00000000);
        }

        // 2. Curățăm fundalul ecranului principal (Desktop-ul)
        gfx_clear(canvas, COLOR_DARK_BG);

        // 3. Desenăm toate ferestrele și bufferele lor în ordine de adâncime
        gfx_wm_draw_all(wm, canvas);

        // 4. Desenăm cursorul la final și facem present pe ecran
        gfx_draw_cursor(canvas, mouse.x, mouse.y);
        gfx_present(canvas, 0, 0);

        // Ieșire la tasta 'q' din tastatură
        if (nano_has_char() && nano_read_char() == 'q') break;
    }

    // Eliberăm resursele la închidere
    gfx_wm_free(wm);
    gfx_free_canvas(canvas);
    nano_set_desktop_mode(0);

    return 0;
}