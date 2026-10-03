#ifndef STRING_H
#define STRING_H

#include <stdint.h>



// O funcție simplă de conversie string -> integer (atoi rudimentar)
static int simple_atoi(const char* str) {
    if (!str) return 0; // Protecție anti-crash
    
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
    if (!buffer) return 0;
    
    char* ptr = buffer;
    
    // Gestionăm manual cazul special INT_MIN pentru a evita overflow-ul la negare
    if (value == -2147483648) {
        // Cel mai simplu e să copiem direct șirul pentru această valoare extremă
        const char* min_str = "-2147483648";
        int i = 0;
        while (min_str[i] != '\0') {
            buffer[i] = min_str[i];
            i++;
        }
        buffer[i] = '\0';
        return buffer;
    }

    if (value < 0) {
        *ptr++ = '-';
        value = -value;
    }

    char* start = ptr;

    do {
        int digit = value % 10;
        *ptr++ = '0' + digit;
        value /= 10;
    } while (value > 0);

    *ptr = '\0';

    // Inversarea părții numerice
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


int snprintf(char* str, uint32_t size, const char* format, ...);
#endif // STRING_H