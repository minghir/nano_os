#include "../io.h"
#include "../syslog.h"

#define MOUSE_DATA_PORT    0x60
#define MOUSE_STATUS_PORT  0x64
#define MOUSE_COMMAND_PORT 0x64

static uint8_t mouse_cycle = 0;
static uint8_t mouse_byte[3];

// Funcție externă definită în noul mouse.c (managerul de input)
extern void mouse_update_position(int dx, int dy, uint8_t buttons);

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
    
    status |= 0x02;  // Activează IRQ 12
    status |= 0x01;  // Activează IRQ 1 (Tastatura) - CRUCIAL!
    status &= ~0x20; // Dezactivează ceasul de mouse dacă e cazul

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
    
    // Deblochează explicit în PIC
	pic_enable_irq(2);
    pic_enable_irq(1);   // Tastatura
    pic_enable_irq(12);  // Mouse-ul
}

// Handlerul apelat la fiecare IRQ 12 (vectorul 44 din IDT)
void mouse_handler_main() {
    // 1. Citim mereu portul 0x60 imediat pentru a elibera bufferul hardware.
    // Dacă nu facem asta, PIC-ul se blochează și mouse-ul îngheață.
    uint8_t data = inb(0x60);

    // 2. Ignorăm pachetele de confirmare (ACK = 0xFA) sau resend (0xFE) trimise de hardware la inițializare
    if (data == 0xFA || data == 0xFE) {
        outb(0xA0, 0x20); 
        outb(0x20, 0x20);
        return;
    }

    // 3. Reconstruim pachetul de 3 octeți
    switch (mouse_cycle) {
        case 0:
            // Sincronizare: Primul octet trebuie să aibă bitul 3 setat (valoare 0x08).
            if (data & 0x08) {
                mouse_byte[0] = data;
                mouse_cycle = 1;
            }
            break;
            
        case 1:
            mouse_byte[1] = data;
            mouse_cycle = 2;
            break;
            
        case 2:
            mouse_byte[2] = data;
            mouse_cycle = 0; // Resetăm ciclul

            // Extragem deplasarea pe axele X și Y
            int dx = (int8_t)mouse_byte[1];
            int dy = (int8_t)mouse_byte[2];

            // Extindem biții de semn pentru direcții negative
            if (mouse_byte[0] & 0x10) dx |= 0xFFFFFF00;
            if (mouse_byte[0] & 0x20) dy |= 0xFFFFFF00;

            uint8_t buttons = mouse_byte[0] & 0x07;

            // Trimitem noile coordonate către logica kernelului
            mouse_update_position(dx, dy, buttons);
            break;
    }

    // 4. Trimitem End of Interrupt (EOI) către ambele cipuri PIC
    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}