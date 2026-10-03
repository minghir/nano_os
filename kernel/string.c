
#include <stdarg.h>

#include "string.h"
#include "io.h"

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

char* string_copy(char* dest, const char* src) {
    char* original_dest = dest;
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0'; // Nu uităm terminatorul de șir!
    return original_dest;
}

void string_concat(char* dest, const char* src) {
    // Mergem până la sfârșitul primului șir
    while (*dest != '\0') {
        dest++;
    }
    // Copiem al doilea șir peste terminatorul primului
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0'; // Punem terminatorul final
}

int starts_with(const char* text, const char* prefix) {
    while (*prefix) {
        if (*text++ != *prefix++) return 0;
    }
    return 1;
}

void print_number(uint32_t value) {
    char buffer[11];
    int index = 10;
    buffer[index] = 0;

    do {
        buffer[--index] = '0' + (value % 10);
        value /= 10;
    } while (value != 0);

    print(&buffer[index]);
}

int string_length(const char* str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

// Funcție simplă de verificare a subșirurilor, ca să nu depindem de alte librării
int string_contains(const char* str, const char* substr) {
    if (!str || !substr) return 0;
    for (int i = 0; str[i] != '\0'; i++) {
        int j = 0;
        while (str[i + j] != '\0' && substr[j] != '\0' && str[i + j] == substr[j]) {
            j++;
        }
        if (substr[j] == '\0') return 1;
    }
    return 0;
}




// Funcție ajutătoare internă pentru conversia unui număr întreg în zecimal
static void _int_to_dec(int64_t val, char* buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    int is_negative = 0;
    if (val < 0) {
        is_negative = 1;
        val = -val;
    }
    char tmp[32];
    int idx = 0;
    while (val > 0) {
        tmp[idx++] = '0' + (val % 10);
        val /= 10;
    }
    int i = 0;
    if (is_negative) {
        buf[i++] = '-';
    }
    while (idx > 0) {
        buf[i++] = tmp[--idx];
    }
    buf[i] = '\0';
}

// Funcție ajutătoare internă pentru conversia în hexazecimal (vital pentru OS)
static void _uint_to_hex(uint64_t val, char* buf, int uppercase) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char tmp[32];
    int idx = 0;
    const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    while (val > 0) {
        tmp[idx++] = digits[val % 16];
        val /= 16;
    }
    int i = 0;
    while (idx > 0) {
        buf[i++] = tmp[--idx];
    }
    buf[i] = '\0';
}

/**
 * O versiune sigură de snprintf pentru mediu bare-metal / kernel.
 * Previne depășirea bufferului și suportă %d, %s, %c, %x, %p.
 */
int snprintf(char* str, uint32_t size, const char* format, ...) {
    if (!str || size == 0 || !format) return 0;

    va_list args;
    va_start(args, format);

    uint32_t written = 0;
    char* ptr = str;
    uint32_t max_len = size - 1; // Păstrăm loc pentru terminatorul '\0'

    int i = 0;
    while (format[i] != '\0' && written < max_len) {
        if (format[i] == '%' && format[i+1] != '\0') {
            i++; // Trecem peste '%'
            char spec = format[i];

            char conv_buf[32];
            char* src = conv_buf;

            if (spec == 'd' || spec == 'i') {
                int64_t val = va_arg(args, int);
                _int_to_dec(val, conv_buf);
            } 
            else if (spec == 'x' || spec == 'X') {
                uint64_t val = va_arg(args, uint64_t);
                _uint_to_hex(val, conv_buf, (spec == 'X'));
            } 
            else if (spec == 'p') {
                // Pentru pointeri afișăm prefixul 0x urmat de valoarea hex
                *ptr++ = '0'; written++;
                if (written < max_len) { *ptr++ = 'x'; written++; }
                uint64_t val = (uint64_t)va_arg(args, void*);
                _uint_to_hex(val, conv_buf, 0);
            } 
            else if (spec == 's') {
                char* s = va_arg(args, char*);
                src = s ? s : "(null)"; // Protecție anti-crash la pointeri NULL
            } 
            else if (spec == 'c') {
                conv_buf[0] = (char)va_arg(args, int);
                conv_buf[1] = '\0';
            } 
            else {
                // Dacă e un caracter necunoscut după %, îl lăsăm ca atare
                conv_buf[0] = '%';
                conv_buf[1] = spec;
                conv_buf[2] = '\0';
            }

            // Copiem șirul convertit în bufferul principal respectând limita
            while (*src != '\0' && written < max_len) {
                *ptr++ = *src++;
                written++;
            }
            i++;
        } else {
            *ptr++ = format[i++];
            written++;
        }
    }

    *ptr = '\0'; // Închidem mereu șirul
    va_end(args);
    return (int)written;
}