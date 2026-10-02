#include "nano_libc.h"

int main(int argc, char* argv[]) {
    nano_print("=== MEM_TEST START ===\n");

    // Test 1: Alocare mică
    nano_print("Test 1: Alocare 64 octeti cu nano_malloc...\n");
    char* ptr = (char*)nano_malloc(64);
    
    if (!ptr) {
        nano_print("EROARE: nano_malloc a returnat NULL (Out of memory)!\n");
        return 1;
    }
    nano_print("Succes: Memorie alocata.\n");

    // Test 2: Scriere în memoria alocată (Aici crapă de obicei dacă memoria nu e mapată user-space)
    nano_print("Test 2: Scriere in memoria alocata...\n");
    string_copy(ptr, "Salut din heap-ul Nano OS!");
    
    nano_print("Continut citit din memorie: ");
    nano_print(ptr);
    nano_print("\n");

    // Test 3: Eliberare
    nano_print("Test 3: Eliberare memorie cu nano_free...\n");
    nano_free(ptr);
    nano_print("Succes: Memorie eliberata.\n");

    nano_print("=== MEM_TEST REUSIT CU SUCCES! ===\n");
    return 0;
}