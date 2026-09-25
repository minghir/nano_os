#include "string.h"

// Funcție utilitară pentru a detecta spațiile (înlocuiește isspace din ctype.h)
static inline int is_space(char c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}

// Funcția trim care modifică buffer-ul in-place
char* trim(char* str) {
    if (!str) return str; // Protecție pentru pointer NULL

    // 1. Trim la stânga (sărim peste spațiile inițiale)
    while (is_space(*str)) {
        str++;
    }

    // Dacă string-ul era format doar din spații, am ajuns la final
    if (*str == '\0') {
        return str;
    }

    // 2. Găsim capătul string-ului (înlocuiește strlen din string.h)
    char* end = str;
    while (*end != '\0') {
        end++;
    }
    end--; // Ne mutăm pe ultimul caracter valid dinaintea lui '\0'

    // 3. Trim la dreapta (mergem înapoi și suprascriem spațiile cu terminator null)
    while (end > str && is_space(*end)) {
        *end = '\0';
        end--;
    }

    return str; // Returnăm noul început al string-ului
}

// ltrim (Left Trim) - Elimină spațiile de la început
char* ltrim(char* str) {
    if (!str) return str; // Protecție pentru NULL

    // Avansăm pointerul până dăm de un caracter care nu e spațiu sau de finalul string-ului
    while (is_space(*str)) {
        str++;
    }

    // Returnăm noul pointer de început
    return str;
}

// rtrim (Right Trim) - Elimină spațiile de la final
char* rtrim(char* str) {
    if (!str) return str; // Protecție pentru NULL

    // Găsim capătul string-ului
    char* end = str;
    while (*end != '\0') {
        end++;
    }

    // Dacă string-ul este deja gol, nu facem nimic
    if (end == str) {
        return str;
    }

    end--; // Ne mutăm pe ultimul caracter valid (înaintea terminatorului '\0')

    // Mergem înapoi spre început și suprascriem spațiile cu '\0'
    // Folosim end >= str pentru a ne asigura că nu ieșim din memoria buffer-ului
    // în cazul în care string-ul conținea DOAR spații.
    while (end >= str && is_space(*end)) {
        *end = '\0';
        end--;
    }

    return str; // rtrim nu modifică începutul, deci returnăm pointerul original
}

// Implementare standard pentru memset cerută de compilator
void* memset(void* dest, int val, int count) {
    unsigned char* ptr = (unsigned char*)dest;
    while (count-- > 0) {
        *ptr++ = (unsigned char)val;
    }
    return dest;
}

// Îți recomand să adaugi și memcpy, compilatorul o va cere curând 
// pentru atribuiri de structuri (ex: struct A = struct B)
void* memcpy(void* dest, const void* src, int count) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (count-- > 0) {
        *d++ = *s++;
    }
    return dest;
}