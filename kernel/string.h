#ifndef STRING_H
#define STRING_H

#include <stdint.h>
#include <stdarg.h>
#include "io.h"

// O funcție simplă de conversie string -> integer
 int simple_atoi(const char* str);

 char* simple_itoa(int value, char* buffer);

 int is_space(char c);
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
 int string_contains(const char* str, const char* substr);

 void _int_to_dec(int64_t val, char* buf);
    
 void _uint_to_hex(uint64_t val, char* buf, int uppercase);
 int snprintf(char* str, uint32_t size, const char* format, ...);

#endif // STRING_H

