#include "idt.h"

struct idt_entry idt[256];
struct idt_ptr idtp;

void idt_set_gate(int num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low  = base & 0xFFFF;
    idt[num].selector    = sel;
    idt[num].zero        = 0;
    idt[num].type_attr   = flags;
    idt[num].offset_mid  = (base >> 16) & 0xFFFF;
    idt[num].offset_high = (base >> 32) & 0xFFFFFFFF;
    idt[num].reserved    = 0;
}

void idt_load() {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint64_t)&idt;

    __asm__ volatile ("lidt %0" : : "m"(idtp));
}

