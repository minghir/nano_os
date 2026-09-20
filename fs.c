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


int fs_create_file(const char* name, uint32_t start_sector, uint32_t size) {
    uint8_t* buffer = (uint8_t*)malloc(512);
    if (!buffer) {
        return 0;
    }

    // Citim Sectorul 1
    disk_read_sector(1, buffer);

    // Verificăm semnătura "NANO"
    if (buffer[0] != 'N' || buffer[1] != 'A' || buffer[2] != 'N' || buffer[3] != 'O') {
        return 0; 
    }

    int entry_size = sizeof(DirectoryEntry);
    int max_entries = (512 - 4) / entry_size;

    // Căutăm o intrare liberă în director
    for (int i = 0; i < max_entries; i++) {
        DirectoryEntry* entry = (DirectoryEntry*)(buffer + 4 + (i * entry_size));

        // Dacă primul octet este 0 sau spațiu, intrarea este liberă
        if (entry->filename[0] == 0 || entry->filename[0] == ' ') {
            
            // Setăm numele (completăm cu spații până la 11 caractere, stil FAT)
            for (int j = 0; j < 11; j++) {
                entry->filename[j] = ' ';
            }
            
            int j = 0;
            while (name[j] != '\0' && j < 11) {
                entry->filename[j] = name[j];
                j++;
            }

            entry->start_sector = start_sector;
            entry->size = size;
            entry->flags = 0;

            // Scriem sectorul actualizat înapoi pe disc
            disk_write_sector(1, buffer);
            return 1; // Succes!
        }
    }

    return 0; // Directorul este plin
}