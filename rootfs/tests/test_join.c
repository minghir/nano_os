#include "nano_libc.h"
#include "nano_string.h"

// Funcția executată de thread-ul copil
void copil_harnic(void* arg) {
    int id = (int)(uint64_t)arg;
    
    nano_print("\n   [Copil] Salut! Sunt noul thread. Am primit argumentul: ");
    nano_print_int(id);
    nano_print("\n");
    
    nano_print("   [Copil] Simulez niste calcule complexe...\n");
    
    // Dacă ai funcție de sleep, folosește-o. Dacă nu, facem un busy-loop scurt pentru întârziere:
    for (volatile int i = 0; i < 50000000; i++) {} 
    
    nano_print("   [Copil] Am terminat treaba! Returnez codul secret: 42\n");
    
    // Părăsim thread-ul lăsând un cod de ieșire
    nano_thread_exit(42);
}

int main() {
    nano_print("[Main] Incepem testul pentru thread_join().\n");

    // 1. Creăm thread-ul
    uint32_t tid = nano_create_thread((void*)copil_harnic, (void*)777);
    
    if (tid == 0) {
        nano_print("[Main] EROARE: Nu am putut crea thread-ul!\n");
        return 1;
    }

    nano_print("[Main] Thread-ul copil a fost creat cu succes. TID: ");
    nano_print_int(tid);
    nano_print("\n");

    nano_print("[Main] Acum intru in sleep (join) asteptand dupa copil...\n");

    // 2. Așteptăm (sincronizare). Aici sistemul ar trebui să treacă `main`-ul în PROC_SLEEPING
    int exit_code = nano_thread_join(tid);

    // 3. Ne-am trezit! Copilul este ZOMBIE și i-am luat codul. Memoria lui a fost eliberată.
    nano_print("\n[Main] M-am trezit! Thread-ul s-a terminat.\n");
    nano_print("[Main] Codul de iesire returnat de copil este: ");
    nano_print_int(exit_code);
    nano_print("\n");

    if (exit_code == 42) {
        nano_print("[Main] TEST TRECUT CU SUCCES! Sincronizarea functioneaza perfect.\n");
    } else {
        nano_print("[Main] TEST PICAT! Codul de iesire nu corespunde.\n");
    }

    return 0;
}