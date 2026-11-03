#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>

void memory_init();
void* malloc(size_t size);
void* malloc_for_process(size_t size, uint32_t pid);
void free(void* ptr);
void free_process_allocations(uint32_t pid);

size_t get_heap_total();
size_t get_heap_used();
size_t get_heap_free();

void* alloc_page();
void free_page(void* ptr);

#endif