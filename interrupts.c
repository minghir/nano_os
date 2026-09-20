#include "io.h"
#include "idt.h"
#include "timer.h"

// Declarăm funcțiile ISR din fișierele de assembly
extern void isr_keyboard();
extern void isr_timer();

void interrupts_init() {
    // 1. Oprește întreruperile global în timpul configurării
    __asm__ volatile ("cli");

    // 2. Remapează PIC-ul (Master: 32-39, Slave: 40-47)
    pic_remap();

    // 3. Setează porțile în IDT
    // Vectorul 32 (IRQ 0) - Timer
    idt_set_gate(32, (uint64_t)isr_timer, 0x08, 0x8E);
    
    // Vectorul 33 (IRQ 1) - Tastatură
    idt_set_gate(33, (uint64_t)isr_keyboard, 0x08, 0x8E);
    
    // Încarcă tabelul IDT în procesor folosind LIDT
    idt_load();

    // 4. Deblochează IRQ 0 (Timer) și IRQ 1 (Tastatură) pe PIC-ul master
    uint8_t mask = inb(0x21);
    mask &= ~((1 << 0) | (1 << 1)); // Deblochează bitul 0 (timer) și bitul 1 (tastatură)
    outb(0x21, mask);

    // 5. Inițializează PIT-ul (Programmable Interval Timer) la 100 Hz
    timer_init(100);

    // 6. Pornește întreruperile global
    __asm__ volatile ("sti");
}