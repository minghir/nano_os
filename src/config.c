#include "config.h"
#include "io.h"

SystemConfig current_config = {
    .timezone_offset = 3, // Valoare implicită (fallback)
    .use_dst = 1
};




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