#ifndef STRING_H
#define STRING_H

#include <stdint.h>

static inline int is_space(char c);
char* trim(char* str);
char* ltrim(char* str);
char* rtrim(char* str);

void* memset(void* dest, int val, int count);
void* memcpy(void* dest, const void* src, int count);

char* string_copy(char* dest, const char* src);
void string_concat(char* dest, const char* src);

int starts_with(const char* text, const char* prefix);

void print_number(uint32_t value);

int string_length(const char* str);
#endif // STRING_H