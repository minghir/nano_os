#ifndef FS_H
#define FS_H

#include <stdint.h>

#define ATTR_FILE      0x01
#define ATTR_DIRECTORY 0x02

// O structură de intrare în director, inspirată deja din FAT (8+3 format pentru nume)
typedef struct {
    char filename[12];     // Numele fișierului sau al folderului
    uint32_t start_sector; // Sectorul de start (date pentru fișier / tabel de intrări pentru folder)
    uint32_t size;         // Dimensiunea fișierului în octeți (0 pentru directoare)
    uint8_t flags;         // Atribute: ATTR_FILE sau ATTR_DIRECTORY
} __attribute__((packed)) DirectoryEntry;


void fs_init();
void fs_list_files();
int fs_create_file(const char* name, uint32_t size);
int fs_delete_file(const char* name);
int fs_write_file(const char* name, const uint8_t* data, uint32_t size);
int fs_read_file(const char* name, uint8_t* buffer, uint32_t max_size);
#endif