/*
#include "paging.h"
#include "memory.h" // Pentru alloc_page()

#define PAGE_MASK 0xFFFFFFFFFFFFF000

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
        uint64_t new_pdp = (uint64_t)alloc_page();
        pml4[pml4_i] = new_pdp | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    }
    uint64_t* pdp = (uint64_t*)(pml4[pml4_i] & PAGE_MASK);

    // 2. Nivelul PDP -> PD
    if (!(pdp[pdp_i] & PAGE_PRESENT)) {
        uint64_t new_pd = (uint64_t)alloc_page();
        pdp[pdp_i] = new_pd | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    }
    uint64_t* pd = (uint64_t*)(pdp[pdp_i] & PAGE_MASK);

    // 3. Nivelul PD -> PT
    if (!(pd[pd_i] & PAGE_PRESENT)) {
        uint64_t new_pt = (uint64_t)alloc_page();
        pd[pd_i] = new_pt | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    }
    uint64_t* pt = (uint64_t*)(pd[pd_i] & PAGE_MASK);

    // 4. Nivelul PT -> Adresa Fizică Reală
    pt[pt_i] = (paddr & PAGE_MASK) | flags;

    // Invalidăm cache-ul TLB al procesorului pt această adresă
    __asm__ volatile("invlpg (%0)" ::"r" (vaddr) : "memory");
}

uint64_t* create_process_pml4() {
    uint64_t* new_pml4 = (uint64_t*)alloc_page();

    // IDENTITY MAPPING (Adresa Virtuală = Adresa Fizică)
    // Mapăm primii 32 MB pentru a include Codul de Kernel, RAM-ul video (0xB8000), 
    // și structurile noastre, altfel când schimbăm tabela de pagini kernelul va da Crash!
    for (uint64_t addr = 0; addr < 0x02000000; addr += PAGE_SIZE) {
        map_page(new_pml4, addr, addr, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }

    return new_pml4;
}

void switch_page_directory(uint64_t* pml4) {
    __asm__ volatile("mov %0, %%cr3" :: "r"((uint64_t)pml4));
}
*/
#include "paging.h"
#include "memory.h" // Pentru alloc_page()

#define PAGE_MASK 0xFFFFFFFFFFFFF000

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
    // Mapăm primii 32 MB pentru a include Codul de Kernel, RAM-ul video (0xB8000), 
    // și structurile noastre, altfel când schimbăm tabela de pagini kernelul va da Crash!
    for (uint64_t addr = 0; addr < 0x02000000; addr += PAGE_SIZE) {
        map_page(new_pml4, addr, addr, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }

    return new_pml4;
}

void switch_page_directory(uint64_t* pml4) {
    __asm__ volatile("mov %0, %%cr3" :: "r"((uint64_t)pml4));
}