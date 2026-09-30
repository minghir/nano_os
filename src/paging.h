#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_SIZE    4096
#define PAGE_PRESENT 0b00000001
#define PAGE_WRITE   0b00000010
#define PAGE_USER    0b00000100

// Funcții
uint64_t* create_process_pml4();
void map_page(uint64_t* pml4, uint64_t vaddr, uint64_t paddr, uint16_t flags);
void switch_page_directory(uint64_t* pml4);

#endif