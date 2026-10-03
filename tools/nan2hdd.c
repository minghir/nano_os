#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define SECTOR_SIZE 512
#define FS_MAGIC "NAN2"

typedef struct {
    char filename[11];
    uint32_t start_sector;
    uint32_t size;
    uint8_t flags;
} __attribute__((packed)) DirectoryEntry;

void read_sector(FILE* disk, uint32_t lba, uint8_t* buffer) {
    fseek(disk, lba * SECTOR_SIZE, SEEK_SET);
    fread(buffer, 1, SECTOR_SIZE, disk);
}

void write_sector(FILE* disk, uint32_t lba, uint8_t* buffer) {
    fseek(disk, lba * SECTOR_SIZE, SEEK_SET);
    fwrite(buffer, 1, SECTOR_SIZE, disk);
}

void format_name(const char* input, char* output) {
    for (int i = 0; i < 11; i++) output[i] = ' ';
    for (int i = 0; i < 11 && input[i] != '\0'; i++) {
        output[i] = input[i];
    }
}

// Găsește sectorul unui director pe baza unei căi (ex: "/test/subfolder")
uint32_t find_dir_sector_by_path(FILE* disk, const char* path) {
    if (!path || path[0] == '\0' || strcmp(path, "/") == 0) {
        return 1; // Rădăcina
    }

    uint32_t current_sec = 1;
    char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy));
    
    char* token = strtok(path_copy, "/");
    while (token != NULL) {
        uint8_t buffer[SECTOR_SIZE];
        read_sector(disk, current_sec, buffer);

        int max_entries = (SECTOR_SIZE - 8) / sizeof(DirectoryEntry);
        int found = 0;
        for (int i = 0; i < max_entries; i++) {
            DirectoryEntry* entry = (DirectoryEntry*)(buffer + 8 + (i * sizeof(DirectoryEntry)));
            if (entry->filename[0] != 0 && entry->filename[0] != ' ') {
                char name[12];
                memcpy(name, entry->filename, 11);
                name[11] = '\0';
                for(int k=10; k>=0; k--) { if(name[k] == ' ') name[k] = '\0'; else break; }

                if (strcmp(name, token) == 0) {
                    if (entry->flags == 1) { // Este director
                        current_sec = entry->start_sector;
                        found = 1;
                        break;
                    } else {
                        // Dacă e ultimul token și căutăm un fișier, oprim căutarea de directoare aici
                        return current_sec;
                    }
                }
            }
        }
        if (!found) {
            return 0; // Nu s-a găsit
        }
        token = strtok(NULL, "/");
    }
    return current_sec;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Utilizare:\n");
        printf("  %s <disk.img> ls [cale]\n", argv[0]);
        printf("  %s <disk.img> push <fisier_linux> <cale_in_nano>\n", argv[0]);
        printf("  %s <disk.img> get <cale_in_nano> <fisier_linux>\n", argv[0]);
        printf("  %s <disk.img> mkdir <cale_noua>\n", argv[0]);
        printf("  %s <disk.img> format\n", argv[0]); // <--- ADAUGAT AICI
        return 1;
    }

    const char* img_path = argv[1];
    const char* command = argv[2];

    FILE* disk = fopen(img_path, "r+b");
    if (!disk) {
        printf("Eroare: Nu pot deschide imaginea %s\n", img_path);
        return 1;
    }
	
	// ---> ADAUGĂ BLOCUL FORMAT AICI <---
    if (strcmp(command, "format") == 0) {
        uint8_t buffer[SECTOR_SIZE];
        memset(buffer, 0, SECTOR_SIZE); // 1. Umplem cu zero

        // 2. Scriem semnătura magică
        memcpy(buffer, FS_MAGIC, 4);

        // 3. Resetăm sectorul liber (începe de la 2)
        uint32_t* next_free = (uint32_t*)(buffer + 4);
        *next_free = 2;

        // 4. Salvăm pe disc (Sector 1)
        write_sector(disk, 1, buffer);

        printf("Succes: Imaginea '%s' a fost formatata complet (Sistem NAN2).\n", img_path);
        fclose(disk);
        return 0; // Ieșim cu succes
    }
	
    uint8_t root_buffer[SECTOR_SIZE];
    read_sector(disk, 1, root_buffer);

    if (memcmp(root_buffer, FS_MAGIC, 4) != 0) {
        printf("Eroare: Imaginea nu este un disc Nano OS valid (lipsește NAN2).\n");
        fclose(disk);
        return 1;
    }

    // --- COMANDA: LS ---
    if (strcmp(command, "ls") == 0) {
        uint32_t target_sector = 1;
        if (argc >= 4) {
            target_sector = find_dir_sector_by_path(disk, argv[3]);
            if (target_sector == 0) {
                printf("Eroare: Calea nu a fost gasita.\n");
                fclose(disk);
                return 1;
            }
        }

        uint8_t dir_buffer[SECTOR_SIZE];
        read_sector(disk, target_sector, dir_buffer);
        printf("--- Continut director (Sector: %d) ---\n", target_sector);
        
        int max_entries = (SECTOR_SIZE - 8) / sizeof(DirectoryEntry);
        for (int i = 0; i < max_entries; i++) {
            DirectoryEntry* entry = (DirectoryEntry*)(dir_buffer + 8 + (i * sizeof(DirectoryEntry)));
            if (entry->filename[0] != 0 && entry->filename[0] != ' ') {
                char name[12];
                memcpy(name, entry->filename, 11);
                name[11] = '\0';
                for(int k=10; k>=0; k--) { if(name[k] == ' ') name[k] = '\0'; else break; }

                printf("%s %s - %d bytes (Sector: %d)\n", 
                    (entry->flags == 1) ? "[DIR] " : "[FILE]", 
                    name, entry->size, entry->start_sector);
            }
        }
    } 
    // --- COMANDA: PUSH (Suportă subdirectoare și suprascriere) ---
    // --- COMANDA: PUSH ---
    else if (strcmp(command, "push") == 0 && argc == 5) {
        // ... parsarea caii ramane la fel ... Pana la ftell(local_f);
        const char* linux_file = argv[3];
        const char* nano_path = argv[4];

        char path_copy[256];
        strncpy(path_copy, nano_path, sizeof(path_copy));
        
        char* last_slash = strrchr(path_copy, '/');
        char dir_path[256] = "/";
        const char* file_name = nano_path;

        if (last_slash != NULL) {
            if (last_slash == path_copy) {
                dir_path[1] = '\0';
            } else {
                *last_slash = '\0';
                strncpy(dir_path, path_copy, sizeof(dir_path));
            }
            file_name = last_slash + 1;
        }

        uint32_t dir_sector = find_dir_sector_by_path(disk, dir_path);
        if (dir_sector == 0) {
            printf("Eroare: Directorul destinație '%s' nu există!\n", dir_path);
            fclose(disk); return 1;
        }

        FILE* local_f = fopen(linux_file, "rb");
        if (!local_f) {
            printf("Eroare: Nu pot citi fișierul Linux '%s'\n", linux_file);
            fclose(disk); return 1;
        }

        fseek(local_f, 0, SEEK_END);
        uint32_t size = ftell(local_f);
        fseek(local_f, 0, SEEK_SET);

        //uint32_t sectors_needed = (size + SECTOR_SIZE - 1) / SECTOR_SIZE;
		uint32_t sectors_needed = size > 0 ? (size + SECTOR_SIZE - 1) / SECTOR_SIZE : 1;

        // 1. ALOCĂM SECTOARELE ÎNTÂI
        uint32_t target_sector;
        uint8_t root_buffer[SECTOR_SIZE];
        read_sector(disk, 1, root_buffer); 
        uint32_t* next_free_ptr = (uint32_t*)(root_buffer + 4);
        target_sector = *next_free_ptr;
        *next_free_ptr += sectors_needed;
        write_sector(disk, 1, root_buffer); // Salvăm noul next_free pe disc

        // 2. CITIM DIRECTORUL
        uint8_t dir_buffer[SECTOR_SIZE];
        read_sector(disk, dir_sector, dir_buffer);

        int max_entries = (SECTOR_SIZE - 8) / sizeof(DirectoryEntry);
        char formatted_name[11];
        format_name(file_name, formatted_name);

		// Căutăm dacă fișierul există deja și îi ștergem intrarea (Overwrite)
        for (int i = 0; i < max_entries; i++) {
            DirectoryEntry* entry = (DirectoryEntry*)(dir_buffer + 8 + (i * sizeof(DirectoryEntry)));
            if (entry->filename[0] != 0 && entry->filename[0] != ' ' && entry->flags == 0) {
                if (memcmp(entry->filename, formatted_name, 11) == 0) {
                    entry->filename[0] = 0; // O marcăm ca ștearsă (liberă)
                    break;
                }
            }
        }

        int success = 0;
        for (int i = 0; i < max_entries; i++) {
            DirectoryEntry* entry = (DirectoryEntry*)(dir_buffer + 8 + (i * sizeof(DirectoryEntry)));
            if (entry->filename[0] == 0 || entry->filename[0] == ' ') {
                memcpy(entry->filename, formatted_name, 11);
                entry->start_sector = target_sector;
                entry->size = size;
                entry->flags = 0; // FILE
                success = 1;
                break;
            }
        }

        if (!success) {
            printf("Eroare: Directorul este plin!\n");
            fclose(local_f); fclose(disk); return 1;
        }

        // AM ȘTERS ALOCAREA DUPLICATĂ DE AICI

        // 3. SCRIEM FIȘIERUL PE DISC
        uint8_t temp_buf[SECTOR_SIZE];
        
        // Ne asigurăm că suntem la începutul fișierului sursă
        fseek(local_f, 0, SEEK_SET);
        
        for (uint32_t i = 0; i < sectors_needed; i++) {
            memset(temp_buf, 0, SECTOR_SIZE); // Umplem restul sectorului cu zero
            
            // Câți octeți ne-au mai rămas de citit din fișier?
            uint32_t to_read = SECTOR_SIZE;
            uint32_t current_pos = i * SECTOR_SIZE;
            if (size - current_pos < SECTOR_SIZE) {
                to_read = size - current_pos;
            }
            
            // Citim exact atâți octeți câți mai sunt utili
            fread(temp_buf, 1, to_read, local_f);
            
            // Scriem întregul sector (512 bytes) pe disc
            write_sector(disk, target_sector + i, temp_buf);
        }

        // 4. ACTUALIZĂM DIRECTORUL
        write_sector(disk, dir_sector, dir_buffer); 

        printf("Succes: '%s' copiat ca '%s' (%d bytes la sectorul %d).\n", linux_file, nano_path, size, target_sector);
        fclose(local_f);
    } 
    // --- COMANDA: GET (Extrage un fișier din Nano OS pe Linux) ---
    else if (strcmp(command, "get") == 0 && argc == 5) {
        const char* nano_path = argv[3];
        const char* linux_file = argv[4];

        char path_copy[256];
        strncpy(path_copy, nano_path, sizeof(path_copy));
        
        char* last_slash = strrchr(path_copy, '/');
        char dir_path[256] = "/";
        const char* file_name = nano_path;

        if (last_slash != NULL) {
            if (last_slash == path_copy) {
                dir_path[1] = '\0';
            } else {
                *last_slash = '\0';
                strncpy(dir_path, path_copy, sizeof(dir_path));
            }
            file_name = last_slash + 1;
        }

        uint32_t dir_sector = find_dir_sector_by_path(disk, dir_path);
        if (dir_sector == 0) {
            printf("Eroare: Calea '%s' nu există!\n", dir_path);
            fclose(disk);
            return 1;
        }

        uint8_t dir_buffer[SECTOR_SIZE];
        read_sector(disk, dir_sector, dir_buffer);

        int max_entries = (SECTOR_SIZE - 8) / sizeof(DirectoryEntry);
        DirectoryEntry* target_entry = NULL;
        char formatted_name[11];
        format_name(file_name, formatted_name);

        for (int i = 0; i < max_entries; i++) {
            DirectoryEntry* entry = (DirectoryEntry*)(dir_buffer + 8 + (i * sizeof(DirectoryEntry)));
            if (entry->filename[0] != 0 && entry->filename[0] != ' ') {
                if (memcmp(entry->filename, formatted_name, 11) == 0) {
                    target_entry = entry;
                    break;
                }
            }
        }

        if (!target_entry || target_entry->flags == 1) {
            printf("Eroare: Fișierul '%s' nu a fost găsit sau este un director!\n", file_name);
            fclose(disk);
            return 1;
        }

        FILE* local_f = fopen(linux_file, "wb");
        if (!local_f) {
            printf("Eroare: Nu pot crea fișierul pe Linux '%s'\n", linux_file);
            fclose(disk);
            return 1;
        }

        uint32_t bytes_remaining = target_entry->size;
        uint32_t current_sec = target_entry->start_sector;
        uint8_t sector_buf[SECTOR_SIZE];

        while (bytes_remaining > 0) {
            read_sector(disk, current_sec, sector_buf);
            uint32_t to_write = bytes_remaining > SECTOR_SIZE ? SECTOR_SIZE : bytes_remaining;
            fwrite(sector_buf, 1, to_write, local_f);
            bytes_remaining -= to_write;
            current_sec++;
        }

        fclose(local_f);
        printf("Succes: S-a extras '%s' în '%s' (%d bytes).\n", nano_path, linux_file, target_entry->size);
    }
    // --- COMANDA: MKDIR (Creează un director) ---
    else if (strcmp(command, "mkdir") == 0 && argc == 4) {
        const char* nano_path = argv[3];

        char path_copy[256];
        strncpy(path_copy, nano_path, sizeof(path_copy));
        
        char* last_slash = strrchr(path_copy, '/');
        char dir_path[256] = "/";
        const char* dir_name = nano_path;

        if (last_slash != NULL) {
            if (last_slash == path_copy) {
                dir_path[1] = '\0';
            } else {
                *last_slash = '\0';
                strncpy(dir_path, path_copy, sizeof(dir_path));
            }
            dir_name = last_slash + 1;
        }

        uint32_t parent_dir_sector = find_dir_sector_by_path(disk, dir_path);
        if (parent_dir_sector == 0) {
            printf("Eroare: Directorul părinte '%s' nu există!\n", dir_path);
            fclose(disk);
            return 1;
        }

        // 1. ALOCĂM SECTORUL ÎNTÂI! (Asta actualizează next_free corect)
        uint32_t target_sector;
        uint8_t root_buffer[SECTOR_SIZE];
        read_sector(disk, 1, root_buffer); 
        uint32_t* next_free_ptr = (uint32_t*)(root_buffer + 4);
        target_sector = *next_free_ptr;
        *next_free_ptr += 1;
        write_sector(disk, 1, root_buffer); // Salvăm noul next_free!

        // 2. ABIA ACUM CITIM DIRECTORUL PĂRINTE!
        uint8_t dir_buffer[SECTOR_SIZE];
        read_sector(disk, parent_dir_sector, dir_buffer);

        int max_entries = (SECTOR_SIZE - 8) / sizeof(DirectoryEntry);
        char formatted_name[11];
        format_name(dir_name, formatted_name);

        int success = 0;
        for (int i = 0; i < max_entries; i++) {
            DirectoryEntry* entry = (DirectoryEntry*)(dir_buffer + 8 + (i * sizeof(DirectoryEntry)));
            if (entry->filename[0] == 0 || entry->filename[0] == ' ') {
                memcpy(entry->filename, formatted_name, 11);
                entry->start_sector = target_sector;
                entry->size = 0;
                entry->flags = 1; // DIR
                success = 1;
                break;
            }
        }

        if (!success) {
            printf("Eroare: Directorul părinte este plin!\n");
            fclose(disk); return 1;
        }

        // 3. Inițializăm noul director cu "DIR2"
        uint8_t temp_buf[SECTOR_SIZE];
        memset(temp_buf, 0, SECTOR_SIZE);
        temp_buf[0] = 'D'; temp_buf[1] = 'I'; temp_buf[2] = 'R'; temp_buf[3] = '2';
        uint32_t* parent_sec_ptr = (uint32_t*)(temp_buf + 4);
        *parent_sec_ptr = parent_dir_sector;
        
        write_sector(disk, target_sector, temp_buf);
        write_sector(disk, parent_dir_sector, dir_buffer); // Acum păstrează corect next_free!

        printf("Succes: Directorul '%s' a fost creat (Sector: %d).\n", nano_path, target_sector);
    }
    else {
        printf("Comandă necunoscută sau argumente lipsă.\n");
    }

    fclose(disk);
    return 0;
}