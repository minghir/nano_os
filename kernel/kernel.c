#include <stdint.h>

static volatile uint16_t* const VGA = (uint16_t*)0xB8000;

void kprint(const char* s) {
    int i = 0;
    while (s[i]) {
        VGA[i] = (uint16_t)s[i] | (uint16_t)(0x0F << 8);
        i++;
    }
}

//void kernel_main(void) {
//    kprint("Salut, Minghir! Kernelul meu minimal.");
//    for (;;);
//}
void kernel_main() {
    char* vga = (char*)0xB8000;

    vga[0] = 'N';   // caracter
    vga[1] = 0x0F;  // alb pe negru

    vga[2] = 'a';
    vga[3] = 0x0F;

    vga[4] = 'n';
    vga[5] = 0x0F;

    vga[6] = 'o';
    vga[7] = 0x0F;

    vga[8] = '!';
    vga[9] = 0x0F;
}
