#include "io.h"
#include "gfx/gfx_term.h"
#include "tty.h"
#define MOUSE_DATA_PORT    0x60
#define MOUSE_STATUS_PORT  0x64
#define MOUSE_COMMAND_PORT 0x64

int mouse_x = 400; // Centrul ecranului implicit
int mouse_y = 300;
uint8_t mouse_buttons = 0;
extern int is_gfx_mode;

static uint8_t mouse_cycle = 0;
static uint8_t mouse_byte[3];

static void mouse_wait_input() {
    uint32_t timeout = 100000;
    while (timeout--) {
        if ((inb(MOUSE_STATUS_PORT) & 2) == 0) return;
    }
}

static void mouse_wait_output() {
    uint32_t timeout = 100000;
    while (timeout--) {
        if ((inb(MOUSE_STATUS_PORT) & 1) != 0) return;
    }
}

static void mouse_write(uint8_t val) {
    mouse_wait_input();
    outb(MOUSE_COMMAND_PORT, 0xD4); // Spune controlerului că următorul byte e pentru mouse
    mouse_wait_input();
    outb(MOUSE_DATA_PORT, val);
}

static uint8_t mouse_read() {
    mouse_wait_output();
    return inb(MOUSE_DATA_PORT);
}

void mouse_init() {
    uint8_t status;

    // Golește bufferul de I/O existent să nu blocheze tastatura
    while (inb(0x64) & 1) {
        inb(0x60);
    }

    // 1. Trimite comanda de activare a portului de mouse la controlerul 8042
    mouse_wait_input();
    outb(0x64, 0xA8);

    // 2. Citește byte-ul de configurare al controlerului 8042
    mouse_wait_input();
    outb(0x64, 0x20);
    mouse_wait_output();
    status = inb(0x60);
    
    // Activează bitul 1 (IMPC - Interrupt from Auxiliary device) 
    // și asigură-te că bitul 0 (Interrupt from Keyboard) este TOT APRINS!
    status |= 0x02; // Activează IRQ 12
    status |= 0x01; // Activează IRQ 1 (Tastatura) - CRUCIAL!
    status &= ~0x20; // Dezactivează ceasul de mouse dacă e cazul, sau lasă-l compatibil

    // Scrie înapoi byte-ul de configurare
    mouse_wait_input();
    outb(0x64, 0x60);
    mouse_wait_input();
    outb(0x60, status);

    // 3. Resetează mouse-ul și pornește raportarea
    mouse_write(0xF6); // Set defaults
    mouse_read();      // ACK

    mouse_write(0xF4); // Enable data reporting
    mouse_read();      // ACK
    
    // Deblochează explicit IRQ 1 (tastatura) și IRQ 12 (mouse-ul) în PIC
    pic_enable_irq(1);  // Tastatura
    pic_enable_irq(12); // Mouse-ul
}

// Handlerul apelat la fiecare IRQ 12 (vectorul 44 din IDT)
// Sus în fișier declarăm un flag vizibil și din alte părți
volatile int mouse_moved = 0;


void mouse_handler_main() {
    uint8_t status = inb(0x64);
    if (!(status & 1)) {
        outb(0xA0, 0x20);
        outb(0x20, 0x20);
        return;
    }

    uint8_t data = inb(0x60);

    switch (mouse_cycle) {
        case 0:
            // Protocolul PS/2: Bitul 3 este întotdeauna 1 pentru primul octet
            if (!(data & 0x08)) {
                break; 
            }
            mouse_byte[0] = data;
            mouse_cycle = 1;
            break;

        case 1:
            mouse_byte[1] = data;
            mouse_cycle = 2;
            break;

        case 2:
            mouse_byte[2] = data;
            mouse_cycle = 0; // Pachet complet de 3 bytes

            uint8_t status_byte = mouse_byte[0];
            int dx = (int8_t)mouse_byte[1];
            int dy = (int8_t)mouse_byte[2];

            // Extinderea semnelor
            if (status_byte & 0x10) { dx |= 0xFFFFFF00; }
            if (status_byte & 0x20) { dy |= 0xFFFFFF00; }

            // Actualizăm poziția
            mouse_x += dx;
            mouse_y -= dy; 

            // Limite ecran (1024x768)
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_x > 1016) mouse_x = 1016;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_y > 756) mouse_y = 756;

            // --- DOAR ANUNȚĂM CĂ S-A MIȘCAT (Fără redesenare grea aici!) ---
            mouse_moved = 1;
            break;
    }

    // Confirmăm întreruperea la ambele PIC-uri
    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

void render_mouse_cursor() {
    if (!is_gfx_mode) return;
    // Poți folosi o funcție din gterm / gfx pentru a desena un mic bloc de 8x8 pixeli la (mouse_x, mouse_y)
    gterm_draw_cursor_box(mouse_x, mouse_y, 0x00FFFFFF); // Exemplu alb
}