#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>

void memory_init();
void* malloc(size_t size);
void free(void* ptr);

size_t get_heap_total();
size_t get_heap_used();
size_t get_heap_free();

#endif