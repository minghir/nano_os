#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

typedef struct {
    int timezone_offset;
    int use_dst;
} SystemConfig;

// Declaram variabila globală de configurare
extern SystemConfig current_config;

// Funcție de parsare a conținutului text din fișierul încărcat
void parse_config(const char* file_data);

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

#endif