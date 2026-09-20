#include "fs.h"
#include "ata.h"
#include "memory.h"

extern void print(const char* s);
extern void newline();

// Funcții externe pentru citire/scriere disc (presupunând că sunt în ata.h)
extern void disk_read_sector(uint32_t lba, uint8_t* buffer);
extern void disk_write_sector(uint32_t lba, const uint8_t* buffer);

void fs_init() {
    // Alocăm un sector în memorie (512 octeți)
    uint8_t* buffer = (uint8_t*)malloc(512);
    if (!buffer) {
        return;
    }

    // Citim Sectorul 1 (unde ținem tabelul de fișiere / metadatele)
    disk_read_sector(1, buffer);

    // Verificăm semnătura "NANO" în primii 4 octeți
    if (buffer[0] != 'N' || buffer[1] != 'A' || buffer[2] != 'N' || buffer[3] != 'O') {
        // Dacă nu există, formatăm discul virtual:
        // 1. Curățăm tot sectorul cu zero
        for (int i = 0; i < 512; i++) {
            buffer[i] = 0;
        }

        // 2. Setăm semnătura "NANO"
        buffer[0] = 'N';
        buffer[1] = 'A';
        buffer[2] = 'N';
        buffer[3] = 'O';

        // 3. Scriem înapoi pe sectorul 1
        disk_write_sector(1, buffer);
    }

    // Notă: Dacă folosești un simplu bump allocator, eliberarea memoriei nu e strict necesară, 
    // dar e bine să păstrăm structura curată.
}


void fs_list_files() {
    uint8_t* buffer = (uint8_t*)malloc(512);
    if (!buffer) {
        print("Eroare: Memorie insuficienta pentru citirea directorului!");
        newline();
        return;
    }

    // Citim Sectorul 1 unde este stocat tabelul de fișiere
    disk_read_sector(1, buffer);

    // Verificăm semnătura "NANO"
    if (buffer[0] != 'N' || buffer[1] != 'A' || buffer[2] != 'N' || buffer[3] != 'O') {
        print("Eroare: Sistemul de fisiere nu este initializat!");
        newline();
        return;
    }

    print("Fisiere existente pe disc:");
    newline();

    // Structura noastră începe după semnătura "NANO" (primii 4 octeți)
    // O intrare are dimensiunea lui DirectoryEntry
    int entry_size = sizeof(DirectoryEntry);
    int max_entries = (512 - 4) / entry_size;

    int found_any = 0;
    for (int i = 0; i < max_entries; i++) {
        // Calculăm offset-ul în buffer (sărim peste primii 4 octeți de "NANO")
        DirectoryEntry* entry = (DirectoryEntry*)(buffer + 4 + (i * entry_size));

        // Dacă primul octet din nume este 0, înseamnă că intrarea e goală/nefolosită
        if (entry->filename[0] != 0 && entry->filename[0] != ' ') {
            found_any = 1;
            
            // Afișăm numele fișierului (afișăm caracter cu caracter pentru siguranță)
            char name_buf[12];
            for (int j = 0; j < 11; j++) {
                name_buf[j] = entry->filename[j];
            }
            name_buf[11] = '\0';

            print(" - ");
            print(name_buf);
            print(" (Dimensiune: ");
            
            // Aici poți adăuga o conversie numerică la șir dacă vrei să afișezi dimensiunea, 
            // deocamdată afișăm doar numele.
            print(" octeti)");
            newline();
        }
    }

    if (!found_any) {
        print("Niciun fisier gasit pe disc.");
        newline();
    }
}

int fs_create_file(const char* name, uint32_t size) {
    uint8_t* buffer = (uint8_t*)malloc(512);
    if (!buffer) {
        return 0;
    }

    // Citim Sectorul 1 (tabelul de directoare)
    disk_read_sector(1, buffer);

    // Verificăm semnătura "NANO"
    if (buffer[0] != 'N' || buffer[1] != 'A' || buffer[2] != 'N' || buffer[3] != 'O') {
        return 0; 
    }

    int entry_size = sizeof(DirectoryEntry);
    int max_entries = (512 - 4) / entry_size;

    // 1. Găsim cel mai mare sector de start folosit până acum, 
    // ca să știm unde îl punem pe următorul.
    uint32_t next_sector = 2; // Începem de la sectorul 2 (după director)
    
    for (int i = 0; i < max_entries; i++) {
        DirectoryEntry* entry = (DirectoryEntry*)(buffer + 4 + (i * entry_size));
        
        // Dacă intrarea este folosită
        if (entry->filename[0] != 0 && entry->filename[0] != ' ') {
            if (entry->start_sector >= next_sector) {
                next_sector = entry->start_sector + 1;
            }
        }
    }

    // 2. Căutăm o intrare liberă în director pentru a adăuga noul fișier
    for (int i = 0; i < max_entries; i++) {
        DirectoryEntry* entry = (DirectoryEntry*)(buffer + 4 + (i * entry_size));

        // Dacă primul octet este 0 sau spațiu, intrarea este liberă
        if (entry->filename[0] == 0 || entry->filename[0] == ' ') {
            
            // Setăm numele (completăm cu spații până la 11 caractere)
            for (int j = 0; j < 11; j++) {
                entry->filename[j] = ' ';
            }
            
            int j = 0;
            while (name[j] != '\0' && j < 11) {
                entry->filename[j] = name[j];
                j++;
            }

            // Atribuim sectorul calculat automat și dimensiunea
            entry->start_sector = next_sector;
            entry->size = size;
            entry->flags = 0;

            // Scriem sectorul actualizat înapoi pe disc
            disk_write_sector(1, buffer);
            return 1; // Succes!
        }
    }

    return 0; // Directorul este plin
}


int fs_write_file(const char* name, const uint8_t* data, uint32_t size) {
    uint8_t* buffer = (uint8_t*)malloc(512);
    if (!buffer) {
        return 0;
    }

    // Citim Sectorul 1 (tabelul de directoare)
    disk_read_sector(1, buffer);

    if (buffer[0] != 'N' || buffer[1] != 'A' || buffer[2] != 'N' || buffer[3] != 'O') {
        return 0; 
    }

    int entry_size = sizeof(DirectoryEntry);
    int max_entries = (512 - 4) / entry_size;

    DirectoryEntry* target_entry = 0;

    // Căutăm fișierul după nume
    for (int i = 0; i < max_entries; i++) {
        DirectoryEntry* entry = (DirectoryEntry*)(buffer + 4 + (i * entry_size));

        // Verificăm dacă numele se potrivește
        int match = 1;
        for (int j = 0; j < 11; j++) {
            char c = name[j];
            if (c == '\0') {
                // Completăm restul cu spații pentru comparație corectă
                while (j < 11) {
                    if (entry->filename[j] != ' ') { match = 0; }
                    j++;
                }
                break;
            }
            if (entry->filename[j] != c) {
                match = 0;
                break;
            }
        }

        if (match) {
            target_entry = entry;
            break;
        }
    }

    if (!target_entry) {
        return 0; // Fișierul nu a fost găsit
    }

    // Pregătim bufferul de date pentru sectorul fișierului (512 octeți)
    uint8_t* sector_data = (uint8_t*)malloc(512);
    if (!sector_data) {
        return 0;
    }

    for (int i = 0; i < 512; i++) {
        sector_data[i] = 0;
    }

    // Copiem datele primite în sectorul dedicat
    uint32_t bytes_to_copy = size < 512 ? size : 512;
    for (uint32_t i = 0; i < bytes_to_copy; i++) {
        sector_data[i] = data[i];
    }

    // Scriem conținutul efectiv pe sectorul alocat fișierului
    disk_write_sector(target_entry->start_sector, sector_data);

    // Actualizăm mărimea în antetul directorului
    target_entry->size = bytes_to_copy;

    // Salvăm modificările din Sectorul 1 înapoi pe disc
    disk_write_sector(1, buffer);

    return 1; // Succes!
}


int fs_read_file(const char* name, uint8_t* buffer, uint32_t max_size) {
    uint8_t* dir_buffer = (uint8_t*)malloc(512);
    if (!dir_buffer) return 0;

    disk_read_sector(1, dir_buffer);

    if (dir_buffer[0] != 'N' || dir_buffer[1] != 'A' || dir_buffer[2] != 'N' || dir_buffer[3] != 'O') {
        return 0; 
    }

    int entry_size = sizeof(DirectoryEntry);
    int max_entries = (512 - 4) / entry_size;
    DirectoryEntry* target_entry = 0;

    for (int i = 0; i < max_entries; i++) {
        DirectoryEntry* entry = (DirectoryEntry*)(dir_buffer + 4 + (i * entry_size));

        int match = 1;
        for (int j = 0; j < 11; j++) {
            char c = name[j];
            if (c == '\0') {
                while (j < 11) {
                    if (entry->filename[j] != ' ') { match = 0; }
                    j++;
                }
                break;
            }
            if (entry->filename[j] != c) {
                match = 0;
                break;
            }
        }

        if (match) {
            target_entry = entry;
            break;
        }
    }

    if (!target_entry || target_entry->start_sector == 0) {
        return 0; // Fișierul nu există
    }

    // Citim sectorul de date al fișierului
    uint8_t* sector_data = (uint8_t*)malloc(512);
    if (!sector_data) return 0;

    disk_read_sector(target_entry->start_sector, sector_data);

    // Copiem în bufferul cerut de utilizator (în limita dimensiunii fișierului)
    uint32_t bytes_to_read = target_entry->size < max_size ? target_entry->size : max_size;
    for (uint32_t i = 0; i < bytes_to_read; i++) {
        buffer[i] = sector_data[i];
    }

    return target_entry->size; // Returnăm dimensiunea citită
}


int fs_delete_file(const char* name) {
    uint8_t* buffer = (uint8_t*)malloc(512);
    if (!buffer) {
        return 0;
    }

    // Citim Sectorul 1 (tabelul de directoare)
    disk_read_sector(1, buffer);

    if (buffer[0] != 'N' || buffer[1] != 'A' || buffer[2] != 'N' || buffer[3] != 'O') {
        return 0; 
    }

    int entry_size = sizeof(DirectoryEntry);
    int max_entries = (512 - 4) / entry_size;

    int found = 0;

    // Căutăm fișierul după nume
    for (int i = 0; i < max_entries; i++) {
        DirectoryEntry* entry = (DirectoryEntry*)(buffer + 4 + (i * entry_size));

        int match = 1;
        for (int j = 0; j < 11; j++) {
            char c = name[j];
            if (c == '\0') {
                while (j < 11) {
                    if (entry->filename[j] != ' ') { match = 0; }
                    j++;
                }
                break;
            }
            if (entry->filename[j] != c) {
                match = 0;
                break;
            }
        }

        if (match) {
            // Marcăm intrarea ca fiind goală (primul octet devine 0)
            entry->filename[0] = 0;
            entry->start_sector = 0;
            entry->size = 0;
            entry->flags = 0;
            found = 1;
            break;
        }
    }

    if (!found) {
        return 0; // Fișierul nu a fost găsit
    }

    // Scriem sectorul actualizat înapoi pe disc
    disk_write_sector(1, buffer);
    return 1; // Succes!
}