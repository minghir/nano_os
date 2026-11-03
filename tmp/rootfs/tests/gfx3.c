#include "nano_libc.h"
#include "nano_string.h"
#include "libgfx.h"


// Structură de date transmisă thread-ului pentru a ține contorul și starea sigură
typedef struct {
    GfxWindow* win;
    int counter;
    volatile int lock; // Mutex simplu pentru acest thread
} WorkerContext;

// --- Worker Thread autonom ---
void window2_worker_thread(void* arg) {
    WorkerContext* ctx = (WorkerContext*)arg;
    char buf[64];

    nano_print("[THREAD] Worker thread a pornit cu succes!\n"); // <--- Adaugă asta

    while (!ctx->win->closed) {
        if (ctx->win->canvas) {
            gfx_clear(ctx->win->canvas, COLOR_WHITE); 
            gfx_draw_string(ctx->win->canvas, 15, 20, "Panou (Thread Corectiv)", COLOR_BLACK, 0x00000000);
            
            snprintf(buf, sizeof(buf), "Tick autonom: %d", ctx->counter++);
            gfx_draw_string(ctx->win->canvas, 15, 45, buf, GFX_RGB(0, 120, 0), 0x00000000);
        }

        for (int i = 0; i < 5000000; i++) { __asm__ volatile("pause"); }
        nano_yield();
    }

    nano_thread_exit(0);
}

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
    gfx_init_window(&win1, 100, 100, 400, 300, "Fereastra Principala (UI)");
    gfx_init_window(&win2, 550, 150, 420, 280, "Fereastra Doi (Multithread)");

    if (win1.canvas) gfx_clear(win1.canvas, COLOR_WHITE);
    // Forțăm o realocare sau verificare a bufferului
if (win2.canvas) {
    // Umplem explicit întregul buffer cu alb din main înainte de orice
    for(int i = 0; i < win2.canvas->width * win2.canvas->height; i++) {
        win2.canvas->buffer[i] = COLOR_WHITE;
    }
}

    gfx_wm_add_window(wm, &win1);
    gfx_wm_add_window(wm, &win2);

    // Inițializăm contextul pentru thread
    WorkerContext ctx;
    ctx.win = &win2;
    ctx.counter = 0;
    ctx.lock = 0;

    // Pornim thread-ul pasând contextul corect
    uint32_t worker_tid = nano_thread_create(window2_worker_thread, &ctx);
    (void)worker_tid;

    MouseState mouse;

    while (1) {
        if (!nano_get_mouse_state(&mouse)) continue;
        if (mouse.buttons & 2) break; 

        gfx_wm_handle_mouse(wm, mouse.x, mouse.y, mouse.buttons);

        // Fereastra 1 este desenată în thread-ul principal
        if (!win1.closed && win1.canvas) {
            gfx_clear(win1.canvas, COLOR_WHITE); 
            gfx_draw_string(win1.canvas, 15, 20, "Aici randeaza thread-ul principal.", COLOR_BLACK, 0x00000000);
            gfx_draw_string(win1.canvas, 15, 45, "Fereastra 2 ruleaza autonom pe kernel thread!", COLOR_BLACK, 0x00000000);
        }

        // Fereastra 2 își actualizează singură canvasul în fundal prin `window2_worker_thread`!

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