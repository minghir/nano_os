#ifndef NANO_LIBC_H
#define NANO_LIBC_H

#include <stdint.h>
#include <stddef.h>  // Pentru size_t

void nano_print(const char* str);
void nano_readline(char* buffer, uint32_t max_len);

void* nano_malloc(size_t size);
void nano_free(void* ptr); // (Chiar dacă e bump allocator, e bine să avem interfața)

#endif