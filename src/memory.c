#include "memory.h"
#include "io.h"

// Am mutat heap-ul mai sus, la 16MB, pentru a evita orice conflict cu kernelul sau stiva
#define HEAP_START 0x01000000 
#define HEAP_SIZE  (1024 * 1024) // 1 Megabyte

static uint8_t* heap_start_ptr = (uint8_t*)HEAP_START;
static uint8_t* heap_current = (uint8_t*)HEAP_START;
static uint8_t* heap_end = (uint8_t*)(HEAP_START + HEAP_SIZE);

void memory_init() {
    heap_current = (uint8_t*)HEAP_START;
    print("Memory manager initialized. Heap start: 0x01000000");
    newline();
}

void* malloc(size_t size) {
    // Aliniem dimensiunea la multipli de 8 octeți pentru arhitectura pe 64-biți
    size = (size + 7) & ~7;

    if (heap_current + size > heap_end) {
        // Out of memory
        return 0; 
    }

    void* ptr = (void*)heap_current;
    heap_current += size;
    return ptr;
}

void free(void* ptr) {
    // Bump allocator-ul simplu nu face nimic la free
    (void)ptr;
}

// Funcții pentru statistici
size_t get_heap_total() {
    return HEAP_SIZE;
}

size_t get_heap_used() {
    return (size_t)(heap_current - heap_start_ptr);
}

size_t get_heap_free() {
    return (size_t)(heap_end - heap_current);
}