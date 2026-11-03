#include "nano_libc.h"
/*
// Funcția care va rula în paralel ca thread independent
void worker_thread(void* arg) {
    nano_print("[THREAD SECUNDAR] Salut! Rulez concurent pe propria stiva.\n");
    
    // Facem câteva iterații ca să lăsăm schedulerul să intercaleze execuția
    for (int i = 1; i <= 3; i++) {
        nano_print("[THREAD SECUNDAR] Lucrez in fundal...\n");
        
        // O buclă scurtă de delay (yield-like / ocupare de procesor)
        for (volatile int d = 0; d < 800000; d++);
    }
    
    nano_print("[THREAD SECUNDAR] Mi-am terminat treaba. Ies curat.\n");
    
    // ATENȚIE: Folosim nano_thread_exit() ca să eliberăm doar stiva acestui thread,
    // fără să distrugem memoria întregului proces principal!
    nano_thread_exit();
}

int main(int argc, char* argv[]) {
    nano_print("=== [TEST MULTITHREADING NANO OS] ===\n");
    
    nano_print("[MAIN] Inainte de crearea thread-ului secundar...\n");
    
    // Creăm thread-ul pasându-i funcția și un argument opțional (NULL)
    uint32_t tid = nano_create_thread(worker_thread, NULL);
    
    if (tid == 0) {
        nano_print("[MAIN] Eroare: Nu s-a putut crea thread-ul!\n");
        nano_sys_exit(1);
    }
    
    nano_print("[MAIN] Thread creat cu succes!\n");
    
    // Firul principal continuă să lucreze în paralel cu thread-ul secundar
    for (int i = 1; i <= 3; i++) {
        nano_print("[MAIN] Firul principal ruleaza in paralel...\n");
        
        for (volatile int d = 0; d < 800000; d++);
    }
    
    nano_print("[MAIN] Test finalizat cu brio. Ies din program.\n");
    nano_sys_exit(0);
}
*/


// Funcția care va rula în paralel ca thread independent
void worker_thread(void* arg) {
    nano_print("[THREAD SECUNDAR] Salut! Rulez concurent pe propria stiva.\n");
    
    for (int i = 1; i <= 5; i++) {
        nano_print("[THREAD SECUNDAR] Lucrez in fundal...\n");
        // O buclă mult mai mare pentru a garanta comutarea de context
        for (volatile int d = 0; d < 50000000; d++); 
    }
    
    nano_print("[THREAD SECUNDAR] Mi-am terminat treaba. Ies curat.\n");
    nano_thread_exit(0);
}

int main(int argc, char* argv[]) {
    nano_print("=== [TEST MULTITHREADING NANO OS] ===\n");
    nano_print("[MAIN] Inainte de crearea thread-ului secundar...\n");
    
    uint32_t tid = nano_create_thread(worker_thread, NULL);
    
    if (tid == 0) {
        nano_print("[MAIN] Eroare: Nu s-a putut crea thread-ul!\n");
        nano_sys_exit(1);
    }
    
    nano_print("[MAIN] Thread creat cu succes!\n");
    
    // Firul principal continuă să lucreze în paralel cu thread-ul secundar
    for (int i = 1; i <= 5; i++) {
        nano_print("[MAIN] Firul principal ruleaza in paralel...\n");
        // Aceeași buclă uriașă de întârziere
        for (volatile int d = 0; d < 50000000; d++);
    }
    
    nano_print("[MAIN] Mi-am terminat treaba. Tin memoria in viata (while 1) pentru copil...\n");
    
    // ATENȚIE: Nu dăm exit() pentru a nu distruge memoria (CR3-ul comun)!
    // Așteptăm la infinit, dar schedulerul va continua să ruleze Thread-ul secundar!
    while(1); 
    
    return 0;
}