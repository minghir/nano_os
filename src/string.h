#ifndef STRING_H
#define STRING_H
static inline int is_space(char c);
char* trim(char* str);
char* ltrim(char* str);
char* rtrim(char* str);

void* memset(void* dest, int val, int count);
void* memcpy(void* dest, const void* src, int count);

#endif // STRING_H