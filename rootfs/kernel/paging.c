#include "paging.h"
#include "memory.h" // Pentru alloc_page()

#define PAGE_MASK 0xFFFFFFFFFFFFF000

// =======================================================
// IMPORTĂM VARIABILELE FRAMEBUFFER-ULUI DIN kernel.c
// =======================================================
extern uint64_t fb_physical_address;
extern uint32_t screen_height;
extern uint32_t screen_pitch;

// Extrage indecșii pentru cele 4 niveluri de paginare din adresa virtuală
static inline uint16_t get_pml4_index(uint64_t vaddr) { return (vaddr >> 39) & 0x1FF; }
static inline uint16_t get_pdp_index(uint64_t vaddr)  { return (vaddr >> 30) & 0x1FF; }
static inline uint16_t get_pd_index(uint64_t vaddr)   { return (vaddr >> 21) & 0x1FF; }
static inline uint16_t get_pt_index(uint64_t vaddr)   { return (vaddr >> 12) & 0x1FF; }

void map_page(uint64_t* pml4, uint64_t vaddr, uint64_t paddr, uint16_t flags) {
    uint16_t pml4_i = get_pml4_index(vaddr);
    uint16_t pdp_i  = get_pdp_index(vaddr);
    uint16_t pd_i   = get_pd_index(vaddr);
    uint16_t pt_i   = get_pt_index(vaddr);

    // 1. Nivelul PML4 -> PDP
    if (!(pml4[pml4_i] & PAGE_PRESENT)) {
        uint64_t* pdp = (uint64_t*)alloc_page();
        for (int i = 0; i < 512; i++) pdp[i] = 0; // CURĂȚĂM GUNOIUL DIN MEMORIE!
        pml4[pml4_i] = (uint64_t)pdp | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    }
    uint64_t* pdp = (uint64_t*)(pml4[pml4_i] & PAGE_MASK);

    // 2. Nivelul PDP -> PD
    if (!(pdp[pdp_i] & PAGE_PRESENT)) {
        uint64_t* pd = (uint64_t*)alloc_page();
        for (int i = 0; i < 512; i++) pd[i] = 0; // CURĂȚĂM GUNOIUL DIN MEMORIE!
        pdp[pdp_i] = (uint64_t)pd | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    }
    uint64_t* pd = (uint64_t*)(pdp[pdp_i] & PAGE_MASK);

    // 3. Nivelul PD -> PT
    if (!(pd[pd_i] & PAGE_PRESENT)) {
        uint64_t* pt = (uint64_t*)alloc_page();
        for (int i = 0; i < 512; i++) pt[i] = 0; // CURĂȚĂM GUNOIUL DIN MEMORIE!
        pd[pd_i] = (uint64_t)pt | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    }
    uint64_t* pt = (uint64_t*)(pd[pd_i] & PAGE_MASK);

    // 4. Nivelul PT -> Adresa Fizică Reală
    pt[pt_i] = (paddr & PAGE_MASK) | flags;

    // Invalidăm cache-ul TLB al procesorului pt această adresă
    __asm__ volatile("invlpg (%0)" ::"r" (vaddr) : "memory");
}

uint64_t* create_process_pml4() {
    uint64_t* new_pml4 = (uint64_t*)alloc_page();
    if (!new_pml4) return 0;

    // CURĂȚĂM COMPLET NOUL PML4 CA SĂ EVITĂM VALORILE REZIDUALE (GARBAGE)
    for (int i = 0; i < 512; i++) {
        new_pml4[i] = 0;
    }

 
    // IDENTITY MAPPING (Adresa Virtuală = Adresa Fizică)
    // Mapăm primii 128 MB pentru a include Kernelul, Video-ul, Heap-ul și noile pagini fizice alocate!
    for (uint64_t addr = 0; addr < 0x08000000; addr += PAGE_SIZE) {
        map_page(new_pml4, addr, addr, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }

    // ==============================================================
    // MAPARE NOUĂ: Adăugăm Framebuffer-ul în spațiul procesului!
    // ==============================================================
    if (fb_physical_address != 0) {
        uint32_t fb_size = screen_pitch * screen_height;
        
        // Aliniem dimensiunea la multiplu de PAGE_SIZE ca să nu lăsăm o pagină pe jumătate mapată
        if (fb_size % PAGE_SIZE != 0) {
            fb_size = (fb_size / PAGE_SIZE + 1) * PAGE_SIZE;
        }
        
        // Mapăm cu identitate toată regiunea unde este placa grafică
        for (uint32_t offset = 0; offset < fb_size; offset += PAGE_SIZE) {
            map_page(new_pml4, fb_physical_address + offset, fb_physical_address + offset, PAGE_PRESENT | PAGE_WRITE);
        }
    }

    return new_pml4;
}

void switch_page_directory(uint64_t* pml4) {
    __asm__ volatile("mov %0, %%cr3" :: "r"((uint64_t)pml4));
}

void free_process_paging(uint64_t pml4_phys) {
    if (!pml4_phys) return;
    uint64_t* pml4 = (uint64_t*)pml4_phys;
    
    for (int i = 0; i < 512; i++) {
        if (pml4[i] & PAGE_PRESENT) {
            uint64_t* pdp = (uint64_t*)(pml4[i] & PAGE_MASK);
            for (int j = 0; j < 512; j++) {
                if (pdp[j] & PAGE_PRESENT) {
                    uint64_t* pd = (uint64_t*)(pdp[j] & PAGE_MASK);
                    for (int k = 0; k < 512; k++) {
                        if (pd[k] & PAGE_PRESENT) {
                            uint64_t* pt = (uint64_t*)(pd[k] & PAGE_MASK);
                            free_page(pt); // Eliberăm tabela Level 1
                        }
                    }
                    free_page(pd); // Eliberăm tabela Level 2
                }
            }
            free_page(pdp); // Eliberăm tabela Level 3
        }
    }
    free_page(pml4); // La final, aruncăm și PML4-ul
}