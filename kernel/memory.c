

#include "memory.h"
#include "io.h"

#define HEAP_START 0x00400000
#define HEAP_SIZE  (2 * 1024 * 1024) // Mărim la 2 MB pentru siguranță

// Antetul fiecărui bloc de memorie din heap
typedef struct BlockHeader {
    size_t size;               // Dimensiunea payload-ului (fără antet)
    int is_free;               // 1 = liber, 0 = ocupat
    struct BlockHeader* next;  // Pointer către următorul bloc din listă
} BlockHeader;

static uint8_t* heap_start_ptr = (uint8_t*)HEAP_START;
static uint8_t* heap_end = (uint8_t*)(HEAP_START + HEAP_SIZE);
static BlockHeader* free_list = 0; // Începutul listei de blocuri

// Alocator de pagini fizice (rămâne stabil)
static uint64_t next_free_physical_page = 0x00600000;

void memory_init() {
    free_list = (BlockHeader*)HEAP_START;
    free_list->size = HEAP_SIZE - sizeof(BlockHeader);
    free_list->is_free = 1;
    free_list->next = 0;

    print("Advanced Heap Manager initialized. Start: 0x00400000, Size: 2MB");
    newline();
}

void* malloc(size_t size) {
    if (size == 0) return 0;

    // Aliniem dimensiunea la multipli de 8 octeți
    size = (size + 7) & ~7;

    BlockHeader* current = free_list;

    // Căutăm primul bloc liber suficient de mare (First-Fit)
    while (current != 0) {
        if (current->is_free && current->size >= size) {
            // Verificăm dacă putem împărți blocul în două (dacă rămâne loc și pentru un antet nou)
            if (current->size >= size + sizeof(BlockHeader) + 8) {
                BlockHeader* next_block = (BlockHeader*)((uint8_t*)(current + 1) + size);
                
                next_block->size = current->size - size - sizeof(BlockHeader);
                next_block->is_free = 1;
                next_block->next = current->next;

                current->size = size;
                current->next = next_block;
            }

            current->is_free = 0;
            // Returnăm adresa de după antet (payload-ul)
            return (void*)(current + 1);
        }
        current = current->next;
    }

    // Out of memory în heap
    return 0;
}

void free(void* ptr) {
    if (!ptr) return;

    // Obținem antetul blocului situat exact cu un struct BlockHeader înaintea datelor
    BlockHeader* header = (BlockHeader*)ptr - 1;
    header->is_free = 1;

    // Opțional: Coalescing (unirea blocurilor libere adiacente pentru a evita fragmentarea)
    BlockHeader* current = free_list;
    while (current != 0 && current->next != 0) {
        if (current->is_free && current->next->is_free) {
            current->size += sizeof(BlockHeader) + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

// Funcții pentru statistici
size_t get_heap_total() {
    return HEAP_SIZE;
}

size_t get_heap_used() {
    size_t used = 0;
    BlockHeader* current = free_list;
    while (current != 0) {
        if (!current->is_free) {
            used += current->size + sizeof(BlockHeader);
        }
        current = current->next;
    }
    return used;
}

size_t get_heap_free() {
    return HEAP_SIZE - get_heap_used();
}

/*
// Alocă 1 pagină fizică (4096 octeți) și o umple cu zero
void* alloc_page() {
	// Să zicem că limităm memoria fizică gestionată la 128 MB (0x08000000)
    if (next_free_physical_page >= 0x08000000) {
        print("KERNEL PANIC: Out of physical memory!");
        while(1) __asm__ volatile("cli; hlt");
    }
	
    uint64_t page_addr = next_free_physical_page;
    next_free_physical_page += 4096;

    uint8_t* ptr = (uint8_t*)page_addr;
    for (int i = 0; i < 4096; i++) {
        ptr[i] = 0;
    }

    return (void*)page_addr;
}
*/

// Pointer către vârful stivei de pagini fizice libere
static void* free_pages_head = 0;

// 1. Funcția îmbunătățită de alocare a unei pagini fizice
void* alloc_page() {
    void* page_addr = 0;

    // A. Dacă avem pagini eliberate anterior, le refolosim cu prioritate!
    if (free_pages_head != 0) {
        page_addr = free_pages_head;
        
        // Următoarea pagină liberă din stivă devine noul "head"
        // Citim pointerul salvat în primii 8 octeți ai paginii curente
        free_pages_head = *(void**)free_pages_head;
    } 
    else {
        // B. Dacă stiva e goală, luăm o pagină nouă din zona liniară (bump allocator)
        // Punem o frână de siguranță la 32MB sau 128MB (în funcție de RAM-ul tău)
        if (next_free_physical_page >= 0x08000000) { 
            print("FATAL KERNEL PANIC: Out of physical memory (RAM)!\n");
            while(1) __asm__ volatile("cli; hlt");
        }

        page_addr = (void*)next_free_physical_page;
        next_free_physical_page += 4096;
    }

    // Curățăm complet pagina cu zero înainte de a o returna
    uint8_t* ptr = (uint8_t*)page_addr;
    for (int i = 0; i < 4096; i++) {
        ptr[i] = 0;
    }

    return page_addr;
}

// 2. Noua funcție de eliberare a unei pagini fizice
void free_page(void* ptr) {
    if (!ptr) return;

    // Verificăm dacă adresa este aliniată la 4096 octeți (securitate)
    uint64_t addr = (uint64_t)ptr;
    if (addr & 0xFFF) {
        print("KERNEL WARNING: Tried to free unaligned physical page!\n");
        return;
    }

    // Opțional: Curățăm datele vechi din pagină din motive de securitate/curățenie
    uint8_t* byte_ptr = (uint8_t*)ptr;
    for (int i = 0; i < 4096; i++) {
        byte_ptr[i] = 0;
    }

    // Adăugăm pagina în stiva de pagini libere (LIFO)
    // În primii 8 octeți ai paginii eliberate, salvăm adresa vechiului head
    *(void**)ptr = free_pages_head;
    
    // Noul head devine pagina curentă pe care o eliberăm
    free_pages_head = ptr;
}