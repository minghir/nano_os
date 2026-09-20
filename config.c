#include "config.h"
#include "io.h"

SystemConfig current_config = {
    .timezone_offset = 3, // Valoare implicită (fallback)
    .use_dst = 1
};

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



void parse_config(const char* file_data) {
    if (!file_data) return;

    const char* ptr = file_data;
    while (*ptr != '\0') {
        if (ptr[0] == 't' && ptr[1] == 'i' && ptr[2] == 'm' && ptr[3] == 'e') {
            while (*ptr != '=' && *ptr != '\0') {
                ptr++;
            }
            if (*ptr == '=') {
                ptr++;
                current_config.timezone_offset = simple_atoi(ptr);
                
                // Mesaj afișat pe ecranul VGA în timpul parsării
                char buf[32];
                simple_itoa(current_config.timezone_offset, buf);

                print("[CONFIG] timezone_offset incarcat cu succes! - Valoare: ");
                print(buf);

                newline();
            }
        }
        ptr++;
    }
}