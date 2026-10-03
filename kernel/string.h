#ifndef STRING_H
#define STRING_H

#include <stdint.h>



// O funcție simplă de conversie string -> integer (atoi rudimentar)
static int simple_atoi(const char* str) {
    int res = 0;
    int sign = 1;
    if (*str == '-') {
        sign = -1;
        str++;
    }
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res * sign;
}

static char* simple_itoa(int value, char* buffer) {
    char* ptr = buffer;

    // Dacă numărul e negativ, îl facem pozitiv și punem '-'
    if (value < 0) {
        *ptr++ = '-';
        value = -value;
    }

    // Salvăm începutul pentru a ști unde începe partea numerică
    char* start = ptr;

    // Convertim numărul în caractere (în ordine inversă)
    do {
        int digit = value % 10;
        *ptr++ = '0' + digit;
        value /= 10;
    } while (value > 0);

    // Terminăm string-ul
    *ptr = '\0';

    // Inversăm partea numerică (pentru că am scris-o invers)
    char* end = ptr - 1;
    while (start < end) {
        char tmp = *start;
        *start = *end;
        *end = tmp;
        start++;
        end--;
    }

    return buffer;
}


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
int string_contains(const char* str, const char* substr);
#endif // STRING_H