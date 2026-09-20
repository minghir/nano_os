#include "io.h"
#include "idt.h"

uint32_t timer_ticks = 0;

// O funcție simplă helper pentru a converti un număr în șir de caractere
static void int_to_str(uint32_t n, char* buf) {
    int i = 0;
    if (n == 0) {
        buf[i++] = '0';
        buf[i] = '\0';
        return;
    }
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    buf[i] = '\0';
    
    // Inversare șir
    int start = 0, end = i - 1;
    while (start < end) {
        char temp = buf[start];
        buf[start] = buf[end];
        buf[end] = temp;
        start++;
        end--;
    }
}

// ISR-ul pentru timer (apelat la fiecare întrerupere IRQ 0)
void timer_irq() {
    timer_ticks++;

    // Actualizăm o dată pe secundă (la fiecare 100 de tick-uri)
    if (timer_ticks % 100 == 0) {
        uint32_t seconds = timer_ticks / 100;
        char time_str[10];
        int_to_str(seconds, time_str);

        // Afișează textul "Sec: " urmat de număr în dreapta sus (Rândul 0, Coloana 70)
        // Poți ajusta coloana în funcție de lungimea textului
        uint16_t* vga = (uint16_t*)0xB8000;
        int index = 0 * 80 + 70; // Rândul 0, Coloana 70
        
        // Scriem eticheta "Sec:"
        char* label = "Sec:";
        while (*label) {
            vga[index++] = (0x0F << 8) | *label++;
        }
        
        // Scriem valoarea secundelor
        char* p = time_str;
        while (*p) {
            vga[index++] = (0x0F << 8) | *p++;
        }
        
        // Dacă numărul scade în lungime, ștergem restul spațiilor
        vga[index] = (0x0F << 8) | ' ';
    }
}

void timer_init(uint32_t frequency) {
    // Calculează divizorul
    uint32_t divisor = 1193180 / frequency;

    // Setează comanda pentru PIT (Canalul 0, Access lobyte/hibyte, Mod 3 - Square Wave Generator)
    outb(0x43, 0x36);

    // Trimite partea low și partea high a divizorului
    uint8_t l = (uint8_t)(divisor & 0xFF);
    uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);

    outb(0x40, l);
    outb(0x40, h);
}