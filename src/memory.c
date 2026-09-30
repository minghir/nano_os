#include "memory.h"
#include "io.h"

// 1. Coborâm Heap-ul la 4 MB (în loc de 16 MB)
#define HEAP_START 0x00400000
#define HEAP_SIZE  (1024 * 1024) // 1 Megabyte


// mem_ptr va începe după primii 32 MB de RAM (lăsăm loc pt Kernel și Heap-ul vechi)
// 2. Coborâm alocatorul de pagini fizice la 6 MB (în loc de 32 MB)
static uint64_t next_free_physical_page = 0x00600000;

static uint8_t* heap_start_ptr = (uint8_t*)HEAP_START;
static uint8_t* heap_current = (uint8_t*)HEAP_START;
static uint8_t* heap_end = (uint8_t*)(HEAP_START + HEAP_SIZE);

void memory_init() {
    heap_current = (uint8_t*)HEAP_START;
    print("Memory manager initialized. Heap start: 0x00400000"); // Am corectat și textul afișat!
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

// Alocă 1 pagină fizică (4096 octeți) și o umple cu zero
void* alloc_page() {
    uint64_t page_addr = next_free_physical_page;
    next_free_physical_page += 4096; // Trecem la următoarea

    // Aici nu va mai crăpa, pentru că 6MB este o adresă "sigură" și validă
    uint8_t* ptr = (uint8_t*)page_addr;
    for (int i = 0; i < 4096; i++) {
        ptr[i] = 0;
    }

    return (void*)page_addr;
}