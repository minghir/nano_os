#include "nano_libc.h"

// Mutex-ul global pentru protejarea ecranului și a resurselor partajate
nano_mutex_t screen_lock = NANO_MUTEX_INIT;

// Contor global partajat între toate thread-urile (protejat de mutex)
volatile int shared_counter = 0;

// Funcția universală pentru worker-threads (primește un ID numeric prin argument)
void worker_thread(void* arg) {
    int thread_id = (int)(intptr_t)arg;
    
    for (int i = 1; i <= 3; i++) {
        // BLOCĂM MUTEX-ul pentru a scrie în siguranță pe ecran și a modifica contorul
        nano_mutex_lock(&screen_lock);
        
        nano_print("[THREAD ");
        nano_print_int(thread_id);
        nano_print("] Iteratia ");
        nano_print_int(i);
        nano_print(" | Counter global: ");
        
        shared_counter++; // Secțiune critică sigură datorită mutex-ului
        nano_print_int(shared_counter);
        nano_print("\n");
        
        nano_mutex_unlock(&screen_lock);
        
        // Yield / Delay simulată pentru a lăsa schedulerul să schimbe contextul
        for (volatile int d = 0; d < 4000000; d++);
    }
    
    nano_mutex_lock(&screen_lock);
    nano_print("[THREAD ");
    nano_print_int(thread_id);
    nano_print("] Mi-am terminat treaba. Ies curat.\n");
    nano_mutex_unlock(&screen_lock);
    
    nano_thread_exit(0);
}

int main(int argc, char* argv[]) {
    nano_print("=== [TEST MULTIPLE THREADS & MUTEX] ===\n");
    nano_print("[MAIN] Lansez 3 thread-uri in paralel...\n");
    
    // Creăm 3 thread-uri distincte, pasându-le ID-ul direct prin argumentul (void*)
    nano_create_thread(worker_thread, (void*)1);
    nano_create_thread(worker_thread, (void*)2);
    nano_create_thread(worker_thread, (void*)3);
    
    nano_print("[MAIN] Toate cele 3 thread-uri au fost lansate!\n");
    
    // Firul principal participă și el la lucru
    for (int i = 1; i <= 3; i++) {
        nano_mutex_lock(&screen_lock);
        nano_print("[MAIN] Firul principal lucreaza in paralel... Counter = ");
        nano_print_int(shared_counter);
        nano_print("\n");
        nano_mutex_unlock(&screen_lock);
        
        for (volatile int d = 0; d < 4000000; d++);
    }
    
    nano_print("[MAIN] Toate task-urile rulează. Tin memoria in viata...\n");
    
    // Ținem programul în viață pentru a permite thread-urilor să termine execuția
    while(1);
    
    return 0;
}