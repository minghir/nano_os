#include <stdint.h>
#include "nano_libc.h"
#include "nano_string.h"

// Declarații pentru funcțiile de afișare
extern void nano_print(const char* str); 
extern void print_number(uint32_t value); // Asigură-te că tipul corespunde cu cel din nano_string.h (uint32_t sau uint64_t)

void run_sse_memory_tests(void) {
    nano_print("[TEST] Pornire teste pentru memcpy & memset SSE...\n");

    // 1. Testul pentru memset
    uint8_t dest_buffer[64];
    
    // Curățăm bufferul cu valoarea 0xAB folosind memset-ul optimizat
    memset(dest_buffer, 0xAB, sizeof(dest_buffer));

    // Verificăm dacă toți octeții au fost setați corect
    int memset_passed = 1;
    for (int i = 0; i < 64; i++) {
        if (dest_buffer[i] != 0xAB) {
            memset_passed = 0;
            break;
        }
    }

    if (memset_passed) {
        nano_print("[TEST] SUCCESS: memset SSE a trecut cu succes!\n");
    } else {
        nano_print("[TEST] ERROR: memset SSE a eșuat la verificare!\n");
    }

    // 2. Testul pentru memcpy
    uint8_t src_buffer[64];
    uint8_t target_buffer[64];

    // Umplem sursa cu un șir de numere de test
    for (int i = 0; i < 64; i++) {
        src_buffer[i] = (uint8_t)i;
        target_buffer[i] = 0; // Resetăm ținta
    }

    // Copiem datele folosind memcpy-ul optimizat SSE
    memcpy(target_buffer, src_buffer, 64);

    // Verificăm dacă datele au fost copiate corect octet cu octet
    int memcpy_passed = 1;
    for (int i = 0; i < 64; i++) {
        if (target_buffer[i] != src_buffer[i]) {
            memcpy_passed = 0;
            nano_print("[TEST] Eșec la indexul: ");
            print_number(i);
            nano_print(" | Sursă: ");
            print_number(src_buffer[i]);
            nano_print(" | Țintă: ");
            print_number(target_buffer[i]);
            nano_print("\n");
            break;
        }
    }

    if (memcpy_passed) {
        nano_print("[TEST] SUCCESS: memcpy SSE a trecut cu succes!\n");
    } else {
        nano_print("[TEST] ERROR: memcpy SSE a eșuat la verificare!\n");
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    nano_print("=== Rulare Test User-Space pentru SSE Memcpy/Memset ===\n");
    
    // Apelăm suita de teste
    run_sse_memory_tests();

    nano_print("=== Teste finalizate. Ieșire din program. ===\n");
    return 0;
}