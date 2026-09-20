#include "io.h"
#include "idt.h"

extern void isr_keyboard();
/*
void interrupts_init() {
    __asm__ volatile ("cli");

    pic_remap();
    idt_set_gate(9, (uint64_t)isr_keyboard, 0x08, 0x8E);
    idt_set_gate(33, (uint64_t)isr_keyboard, 0x08, 0x8E);
    idt_load();

    pic_enable_irq(1);
    __asm__ volatile ("sti");
}
*/

void interrupts_init() {
    // 1. Oprește întreruperile global în timpul configurării
    __asm__ volatile ("cli");

    // 2. Remapează PIC-ul (Master: 32-39, Slave: 40-47)
    pic_remap();

    // 3. Setează poarta pentru tastatură pe vectorul 33 (IRQ 1)
    // 0x08 = selectorul de cod din GDT, 0x8E = tipul (Interrupt Gate pe 64-biți, prezent)
    idt_set_gate(33, (uint64_t)isr_keyboard, 0x08, 0x8E);
    
    // Încarcă tabelul IDT în procesor folosind LIDT
    idt_load();

    // 4. Deblochează doar IRQ 1 (Tastatura) pe PIC-ul master
    pic_enable_irq(1);

    // 5. Pornește întreruperile global
    __asm__ volatile ("sti");
}
