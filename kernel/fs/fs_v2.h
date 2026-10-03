#ifndef FS_V2_H
#define FS_V2_H

#include <stdint.h>

// Definim flag-urile pentru a distinge între fișiere și directoare
#define FS_FLAG_FILE 0x00
#define FS_FLAG_DIR  0x01

// Structura unei intrări din director (format fix de 16 octeți pentru eficiență)
typedef struct {
    char filename[11];     // Numele (padat cu spații, ex: "TEST       ")
    uint32_t start_sector; // Sectorul de pe disc unde încep datele / subdirectorul
    uint32_t size;         // Dimensiunea fișierului în octeți (0 pentru directoare)
    uint8_t flags;         // 0 = Fișier (FS_FLAG_FILE), 1 = Director (FS_FLAG_DIR)
} __attribute__((packed)) DirectoryEntry;

// --- Funcții principale ale sistemului de fișiere ---
void fsv2_init();
void fsv2_list_files(const char* path);

// --- Funcții pentru manipularea fișierelor ---
int fsv2_create_file(const char* name, uint32_t size);
int fsv2_delete_file(const char* name);
int fsv2_write_file(const char* name, const uint8_t* data, uint32_t size);
int fsv2_read_file(const char* name, uint8_t* buffer, uint32_t max_size);

// --- Funcții pentru manipularea directoarelor ---
int fsv2_mkdir(const char* name);
int fsv2_rmdir(const char* name);
int fsv2_cd(const char* name);
void fsv2_get_current_path(char* buffer, uint32_t max_len);

void fsv2_fdisk();
void fsv2_format();

int fsv2_get_stats(uint32_t* total_sectors, uint32_t* free_sectors);


#endif