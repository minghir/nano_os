#include "nano_libc.h"

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}




int atoi(const char* str) {
    int res = 0;
    int sign = 1;
    int i = 0;

    // Ignorăm spațiile albe de la început
    while (str[i] == ' ' || str[i] == '\t' || str[i] == '\n' || str[i] == '\r') {
        i++;
    }

    // Gestionăm semnul
    if (str[i] == '-') {
        sign = -1;
        i++;
    } else if (str[i] == '+') {
        i++;
    }

    // Convertim cifrele
    while (str[i] >= '0' && str[i] <= '9') {
        res = res * 10 + (str[i] - '0');
        i++;
    }

    return res * sign;
}

char* itoa(int value, char* str, int base) {
    char* rc;
    char* ptr;
    char* low;
    // Doar bazele suportate uzual (de la 2 la 36)
    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }

    ptr = str;
    // Gestionăm numere negative doar pentru baza 10
    if (value < 0 && base == 10) {
        *ptr++ = '-';
        value = -value;
    }

    rc = ptr;

    // Generăm cifrele invers
    do {
        int t = value % base;
        if (t >= 10) {
            *ptr++ = 'a' + (t - 10);
        } else {
            *ptr++ = '0' + t;
        }
        value /= base;
    } while (value > 0);

    *ptr-- = '\0';

    // Inversăm șirul obținut pentru a fi în ordine corectă
    low = rc;
    while (low < ptr) {
        char tmp = *low;
        *low++ = *ptr;
        *ptr-- = tmp;
    }

    return str;
}




// în nano_libc.c / nano_libc.h



// Funcție custom pentru a converti string la int64_t (fără dependențe de atoll din libc)



