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

        int tmp = counter;

        nano_yield();

        tmp++;

        nano_yield();

        counter = tmp;

        nano_mutex_unlock(&lock);

        if((i % 100) == 0)
            nano_yield();
    }

    nano_print("[T");
    nano_print_int(id);
    nano_print("] done\n");

    nano_thread_exit(0);
}

int main()
{
    nano_print("=== MUTEX STRESS TEST ===\n");

    for(int i = 0; i < THREADS; i++)
    {
        nano_create_thread(worker, (void*)(intptr_t)(i + 1));
    }

    while(1)
    {
        nano_print("Counter = ");
        nano_print_int(counter);
        nano_print("\n");

        for(volatile int d = 0; d < 10000000; d++);
    }

    return 0;
}
