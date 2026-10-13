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
    // 1. Golește bufferul de I/O
    while (inb(0x64) & 1) {
        inb(0x60);
    }

    // 2. Activează portul auxiliar (mouse-ul) în controlerul 8042
    mouse_wait_input();
    outb(0x64, 0xA8);

    // 3. Setează byte-ul de configurare al controlerului 8042
    mouse_wait_input();
    outb(0x64, 0x20);
    mouse_wait_output();
    uint8_t status = inb(0x60);
    
    status |= 0x02;  // Activează IRQ 12
    status |= 0x01;  // Activează IRQ 1 (Tastatura)
    status &= ~0x20; // Activează ceasul de mouse

    mouse_wait_input();
    outb(0x64, 0x60);
    mouse_wait_input();
    outb(0x60, status);

    // 4. Trimite setările către mouse FĂRĂ SĂ BLOCĂM KERNELUL DACĂ RĂSPUNSUL ÎNTÂRZIE
    mouse_write(0xF6); // Set defaults
    mouse_read();      // Citim ACK-ul (și îl ignorăm dacă e timeout)

    mouse_write(0xF4); // Enable data reporting - CRUCIAL!
    mouse_read();      // Citim ACK-ul

    // 5. Deblocare explicită în PIC
    pic_enable_irq(2);  // Cascada Slave PIC
    pic_enable_irq(1);  // Tastatura
    pic_enable_irq(12); // Mouse-ul
}

// Handlerul apelat la fiecare IRQ 12 (vectorul 44 din IDT)
void mouse_handler_main() {
    uint8_t status = inb(MOUSE_STATUS_PORT);
    if (!(status & 0x01) || !(status & 0x20)) {
        outb(0xA0, 0x20);
        outb(0x20, 0x20);
        return;
    }

    uint8_t data = inb(MOUSE_DATA_PORT);

    if (data == 0xFA || data == 0xFE) {
        outb(0xA0, 0x20); 
        outb(0x20, 0x20);
        return;
    }

    if (mouse_cycle == 0) {
        // Bitul 3 este setat întotdeauna în primul byte al pachetului PS/2.
        if (data & 0x08) {
            mouse_byte[0] = data;
            mouse_cycle = 1;
        }
    } else if (mouse_cycle == 1) {
        mouse_byte[1] = data;
        mouse_cycle = 2;
    } else {
        mouse_byte[2] = data;
        mouse_cycle = 0;
		/*
        // Pachetul marchează overflow pe axele X/Y; valorile nu sunt utilizabile.
        if (!(mouse_byte[0] & 0xC0)) {
            int dx = (int8_t)mouse_byte[1];
            int dy = (int8_t)mouse_byte[2];

            uint8_t buttons = mouse_byte[0] & 0x07;
            mouse_update_position(dx, dy, buttons);
        }
		*/
		// Pachetul marchează overflow pe axele X/Y; valorile nu sunt utilizabile.
        if (!(mouse_byte[0] & 0xC0)) {
            int dx = mouse_byte[1];
            int dy = mouse_byte[2];

            // Extindem semnul manual folosind biții din Byte 0
            if (mouse_byte[0] & 0x10) dx |= 0xFFFFFF00;
            if (mouse_byte[0] & 0x20) dy |= 0xFFFFFF00;

            uint8_t buttons = mouse_byte[0] & 0x07;
            mouse_update_position(dx, dy, buttons);
        }
    }

    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}