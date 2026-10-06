#include "nano_libc.h"

// Variabila partajată (resursa critică)
nano_mutex_t screen_lock = NANO_MUTEX_INIT;

void worker_thread(void* arg) {
    for (int i = 0; i < 3; i++) {
        // BLOCĂM RESURSA
        nano_mutex_lock(&screen_lock);
        //nano_mutex_spinlock(&screen_lock);
        nano_print("[THREAD] Am luat lacatul! Scriu in liniste...\n");
        for (volatile int d = 0; d < 5000000; d++); // Simulăm lucru pe resursă
        nano_print("[THREAD] Eliberez lacatul.\n");
        
        // DEBLOCĂM RESURSA
        nano_mutex_unlock(&screen_lock);
        
        // Așteptăm un pic neblocați ca să dăm șansa și părintelui
        for (volatile int d = 0; d < 5000000; d++); 
    }
    nano_thread_exit();
}

int main() {
    nano_create_thread(worker_thread, NULL);
    
    for (int i = 0; i < 3; i++) {
        // BLOCĂM RESURSA
        nano_mutex_lock(&screen_lock);
        
        nano_print("[MAIN] Am luat lacatul! Niciun thread nu ma poate intrerupe pe ecran.\n");
        for (volatile int d = 0; d < 5000000; d++); // Simulăm lucru
        nano_print("[MAIN] Eliberez lacatul.\n");
        
        // DEBLOCĂM RESURSA
        nano_mutex_unlock(&screen_lock);
        
        for (volatile int d = 0; d < 5000000; d++); 
    }
    
    while(1); 
    return 0;
}