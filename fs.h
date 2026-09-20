#ifndef FS_H
#define FS_H

#include <stdint.h>

// O structură de intrare în director, inspirată deja din FAT (8+3 format pentru nume)
typedef struct {
    char filename[11];    // Nume + Extensie (ex: "TEST    TXT")
    uint32_t start_sector; // Sectorul de unde începe fișierul pe disc
    uint32_t size;        // Dimensiunea fișierului în octeți
    uint8_t flags;        // Atribute (fișier, director etc.)
} __attribute__((packed)) DirectoryEntry;

void fs_init();
void fs_list_files();
int fs_create_file(const char* name, uint32_t start_sector, uint32_t size);
#endif