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

#endif