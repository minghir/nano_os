#include "nano_libc.h"

#define THREADS 50
#define ITERATIONS 1000

volatile int counter = 0;

nano_mutex_t lock = NANO_MUTEX_INIT;

void worker(void* arg)
{
    int id = (int)(intptr_t)arg;

    for(int i = 0; i < ITERATIONS; i++)
    {
        nano_mutex_lock(&lock);
        // Zona critică trebuie să fie cât mai scurtă și rapidă!
        int tmp = counter;
        tmp++;
        counter = tmp;
        nano_mutex_unlock(&lock);

        // După ce dă drumul la lacăt, poate să mai lase și pe alții
        if((i % 100) == 0)
            nano_yield();
    }

    nano_print("[T");
    nano_print_int(id);
    nano_print("] ");
    nano_thread_exit(0);
}

int main()
{
    nano_print("=== MUTEX STRESS TEST ===\n");
    
    // Păstrăm TID-urile ca să le putem aștepta
    uint32_t tids[THREADS];

    for(int i = 0; i < THREADS; i++)
    {
        tids[i] = nano_create_thread(worker, (void*)(intptr_t)(i + 1));
    }

    nano_print("Toate cele 50 de thread-uri au fost pornite.\n");
    nano_print("Main-ul intra la somn pana termina ele...\n");

    // Acum folosim IPC-ul pe care tocmai l-ai creat!
    // Așteptăm civilizat să se termine fiecare thread.
    for(int i = 0; i < THREADS; i++)
    {
        nano_thread_join(tids[i]);
    }

    // Aici ajungem DOAR după ce au terminat toți 50.
    nano_print("\n=== TEST FINALIZAT ===\n");
    nano_print("Counter final = ");
    nano_print_int(counter); // Aici trebuie să vezi EXACT 50000 (50 * 1000)
    nano_print("\n");

    return 0;
}
