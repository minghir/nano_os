#include "fs.h"
#include "io.h"
#include "shell.h"
#include "ata.h"
#include "memory.h"

extern void print(const char* s);
extern void newline();
extern void disk_read_sector(uint32_t lba, uint8_t* buffer);
extern void disk_write_sector(uint32_t lba, const uint8_t* buffer);

// Semnătura nouă. Când o schimbăm, forțăm reformatarea discului virtual.
#define FS_MAGIC "NAN2"
#define DIR_MAGIC "DIR2"

// Starea curentă a sistemului de fișiere (navigația)
static uint32_t current_dir_sector = 1;

// --- Funcții utilitare ---

// Compară două string-uri (până la \0)
static int streq(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

// Formatează numele (padat cu spații)
static void set_entry_filename(DirectoryEntry* entry, const char* name) {
    for (int j = 0; j < 11; j++) entry->filename[j] = ' ';
    int j = 0;
    while (name[j] != '\0' && j < 11) {
        entry->filename[j] = name[j];
        j++;
    }
}

// Caută o intrare în directorul CURENT
static DirectoryEntry* find_file_entry(uint8_t* dir_buffer, const char* name) {
    int max_entries = (512 - 8) / sizeof(DirectoryEntry);
    
    for (int i = 0; i < max_entries; i++) {
        DirectoryEntry* entry = (DirectoryEntry*)(dir_buffer + 8 + (i * sizeof(DirectoryEntry)));
        if (entry->filename[0] == 0 || entry->filename[0] == ' ') continue;

        int match = 1;
        for (int j = 0; j < 11; j++) {
            char c = name[j];
            if (c == '\0') {
                while (j < 11) { if (entry->filename[j] != ' ') match = 0; j++; }
                break;
            }
            if (entry->filename[j] != c) { match = 0; break; }
        }
        if (match) return entry;
    }
    return 0;
}

// Alocă N sectoare de pe disc folosind metadatele din Sectorul 1 (Root)
static uint32_t allocate_sectors(uint32_t count) {
    uint8_t root_buffer[512];
    disk_read_sector(1, root_buffer);
    
    uint32_t* next_free = (uint32_t*)(root_buffer + 4);
    uint32_t allocated = *next_free;
    *next_free += count;
    
    disk_write_sector(1, root_buffer);
    return allocated;
}

// --- Operații de bază ---

void fs_init() {
    uint8_t buffer[512];
    disk_read_sector(1, buffer);

    if (buffer[0] != FS_MAGIC[0] || buffer[1] != FS_MAGIC[1] || buffer[2] != FS_MAGIC[2] || buffer[3] != FS_MAGIC[3]) {
        for (int i = 0; i < 512; i++) buffer[i] = 0;
        
        buffer[0] = FS_MAGIC[0]; buffer[1] = FS_MAGIC[1]; 
        buffer[2] = FS_MAGIC[2]; buffer[3] = FS_MAGIC[3];
        
        // Offset 4: Următorul sector liber global (începe de la 2)
        uint32_t* next_free = (uint32_t*)(buffer + 4);
        *next_free = 2;

        disk_write_sector(1, buffer);
        print("Sistem de fisiere formatat (V2).");
        newline();
    }
    current_dir_sector = 1; // Resetăm locația la root
}

void fs_list_files() {
    uint8_t buffer[512];
    disk_read_sector(current_dir_sector, buffer);

    int max_entries = (512 - 8) / sizeof(DirectoryEntry);
    int found_any = 0;
    
    print("Continut director:"); newline();

    for (int i = 0; i < max_entries; i++) {
        DirectoryEntry* entry = (DirectoryEntry*)(buffer + 8 + (i * sizeof(DirectoryEntry)));

        if (entry->filename[0] != 0 && entry->filename[0] != ' ') {
            found_any = 1;
            char name_buf[12];
            for (int j = 0; j < 11; j++) name_buf[j] = entry->filename[j];
            name_buf[11] = '\0';

            print(entry->flags == FS_FLAG_DIR ? "[DIR]  " : "[FILE] ");
            print(name_buf);
            newline();
        }
    }

    if (!found_any) { print(" (Director gol)"); newline(); }
}

// --- Gestiunea Directoarelor ---

int fs_mkdir(const char* name) {
    // 1. ALOCĂM ÎNTÂI SECTORUL! (Asta actualizează Sectorul 1 direct pe disc)
    uint32_t new_dir_sec = allocate_sectors(1);

    // 2. ABIA ACUM citim directorul curent în buffer (acum va conține noile valori de sistem)
    uint8_t buffer[512];
    disk_read_sector(current_dir_sector, buffer);

    if (find_file_entry(buffer, name) != 0) return 0; // Există deja

    int max_entries = (512 - 8) / sizeof(DirectoryEntry);
    for (int i = 0; i < max_entries; i++) {
        DirectoryEntry* entry = (DirectoryEntry*)(buffer + 8 + (i * sizeof(DirectoryEntry)));
        if (entry->filename[0] == 0 || entry->filename[0] == ' ') {
            
            set_entry_filename(entry, name);
            entry->start_sector = new_dir_sec;
            entry->size = 0;
            entry->flags = FS_FLAG_DIR;
            disk_write_sector(current_dir_sector, buffer); // Salvăm părintele

            // Inițializăm noul sector (directorul copil)
            uint8_t child_buffer[512] = {0};
            child_buffer[0] = DIR_MAGIC[0]; child_buffer[1] = DIR_MAGIC[1];
            child_buffer[2] = DIR_MAGIC[2]; child_buffer[3] = DIR_MAGIC[3];
            
            uint32_t* parent_sec = (uint32_t*)(child_buffer + 4);
            *parent_sec = current_dir_sector;
            
            disk_write_sector(new_dir_sec, child_buffer);
            return 1;
        }
    }
    return 0; // Plin
}

int fs_cd(const char* path) {
    if (streq(path, "/")) {
        current_dir_sector = 1;
        return 1;
    }

    uint32_t target_sector = current_dir_sector;
    
    // Dacă calea începe cu '/', navigarea începe din Root (Sectorul 1)
    if (path[0] == '/') {
        target_sector = 1;
        path++; // Sărim peste primul slash
    }

    // Spargem calea bucată cu bucată
    while (*path) {
        // Sărim peste slash-uri multiple (ex: //test)
        while (*path == '/') path++;
        if (*path == '\0') break;

        // Extragem numele următorului folder
        char folder[12];
        int i = 0;
        while (*path != '/' && *path != '\0' && i < 11) {
            folder[i++] = *path++;
        }
        folder[i] = '\0';

        // Dacă numele era mai lung de 11 caractere, sărim restul ca să ajungem la '/'
        while (*path != '/' && *path != '\0') path++;

        // Tratăm cazul ".."
        if (streq(folder, "..")) {
            if (target_sector != 1) {
                uint8_t buffer[512];
                disk_read_sector(target_sector, buffer);
                uint32_t* parent_sec = (uint32_t*)(buffer + 4);
                
                target_sector = *parent_sec;
                
                // PROTECȚIA: Dacă fișierul e vechi și ne trimite în Sectorul 0,
                // forțăm întoarcerea în Rădăcină (Sectorul 1).
                if (target_sector < 1) {
                    target_sector = 1;
                }
            }
            continue;
        }
        
        // Ignorăm "." (directorul curent)
        if (streq(folder, ".")) continue;

        // Căutăm folderul în sectorul target curent
        uint8_t buffer[512];
        disk_read_sector(target_sector, buffer);
        DirectoryEntry* entry = find_file_entry(buffer, folder);

        if (entry && entry->flags == FS_FLAG_DIR) {
            target_sector = entry->start_sector; // Avansăm în subdirector
        } else {
            return 0; // Eroare: Unul dintre foldere din cale nu există
        }
    }

    // Dacă toată bucla a rulat cu succes, actualizăm locația globală
    current_dir_sector = target_sector;
    return 1;
}

int fs_rmdir(const char* name) {
    uint8_t buffer[512];
    disk_read_sector(current_dir_sector, buffer);
    DirectoryEntry* entry = find_file_entry(buffer, name);

    // Poate fi ștearsă doar dacă e director. (Pentru simplitate, momentan nu verificăm dacă e gol)
    if (entry && entry->flags == FS_FLAG_DIR) {
        entry->filename[0] = 0;
        disk_write_sector(current_dir_sector, buffer);
        return 1;
    }
    return 0;
}

// --- Gestiunea Fișierelor (Adaptate) ---

int fs_create_file(const char* name, uint32_t size) {
    uint32_t sectors_needed = size > 0 ? (size + 511) / 512 : 1;
    
    // 1. ALOCĂM ÎNTÂI SECTOARELE!
    uint32_t new_file_sec = allocate_sectors(sectors_needed);

    // 2. CITIM DUPĂ ALOCARE!
    uint8_t buffer[512];
    disk_read_sector(current_dir_sector, buffer);
    if (find_file_entry(buffer, name) != 0) return 0;

    int max_entries = (512 - 8) / sizeof(DirectoryEntry);
    for (int i = 0; i < max_entries; i++) {
        DirectoryEntry* entry = (DirectoryEntry*)(buffer + 8 + (i * sizeof(DirectoryEntry)));
        if (entry->filename[0] == 0 || entry->filename[0] == ' ') {
            
            set_entry_filename(entry, name);
            entry->start_sector = new_file_sec;
            entry->size = size;
            entry->flags = FS_FLAG_FILE;

            disk_write_sector(current_dir_sector, buffer);
            return 1;
        }
    }
    return 0;
}

int fs_write_file(const char* name, const uint8_t* data, uint32_t size) {
    uint8_t buffer[512];
    disk_read_sector(current_dir_sector, buffer);
    
    DirectoryEntry* entry = find_file_entry(buffer, name);
    if (!entry || entry->flags == FS_FLAG_DIR) return 0;

    uint8_t sector_data[512] = {0}; 
    uint32_t bytes_to_copy = size < 512 ? size : 512;
    for (uint32_t i = 0; i < bytes_to_copy; i++) sector_data[i] = data[i];

    disk_write_sector(entry->start_sector, sector_data);
    entry->size = bytes_to_copy;
    disk_write_sector(current_dir_sector, buffer);
    return 1;
}

int fs_read_file(const char* name, uint8_t* buffer, uint32_t max_size) {
    uint8_t dir_buffer[512];
    disk_read_sector(current_dir_sector, dir_buffer);
    
    DirectoryEntry* entry = find_file_entry(dir_buffer, name);
    if (!entry || entry->flags == FS_FLAG_DIR || entry->start_sector == 0) return 0;

    uint32_t bytes_to_read = entry->size < max_size ? entry->size : max_size;
    
    // Calculăm câte sectoare ocupă fișierul
    uint32_t sectors_to_read = (bytes_to_read + 511) / 512;
    
    // Citim sector cu sector direct în bufferul țintă (ex: în memorie la 0x800000)
    uint32_t total_read = 0;
    for (uint32_t s = 0; s < sectors_to_read; s++) {
        uint8_t sector_data[512];
        disk_read_sector(entry->start_sector + s, sector_data);
        
        // Copiem octet cu octet din sector în bufferul destinație
        for (uint32_t i = 0; i < 512 && total_read < bytes_to_read; i++) {
            buffer[total_read++] = sector_data[i];
        }
    }
    
    return entry->size;
}

int fs_delete_file(const char* name) {
    uint8_t buffer[512];
    disk_read_sector(current_dir_sector, buffer);
    DirectoryEntry* entry = find_file_entry(buffer, name);

    if (entry && entry->flags == FS_FLAG_FILE) {
        entry->filename[0] = 0;
        disk_write_sector(current_dir_sector, buffer);
        return 1;
    }
    return 0;
}

void fs_fdisk() {
    uint8_t mbr_buffer[512];
    uint8_t fs_buffer[512];

    // Citim Sectorul 0 (Bootloader / MBR)
    disk_read_sector(0, mbr_buffer);
    
    // Citim Sectorul 1 (Sistemul de fișiere)
    disk_read_sector(1, fs_buffer);

    print("--- Analiza Hard Disk (fdisk) ---"); newline();

    // 1. Verificăm semnătura de boot (0x55AA la finalul MBR-ului)
    print("Sector 0 (MBR)  : ");
    if (mbr_buffer[510] == 0x55 && mbr_buffer[511] == 0xAA) {
        print("Bootable (0x55AA gasit)");
    } else {
        print("Data Disk (Nu este bootabil)");
    }
    newline();

    // 2. Verificăm sistemul de fișiere
    print("Sistem Fisiere  : ");
    if (fs_buffer[0] == 'N' && fs_buffer[1] == 'A' && fs_buffer[2] == 'N' && fs_buffer[3] == '2') {
        print("NAN2 (Nano OS FS)"); newline();
        
        // Extragem câte sectoare s-au consumat pe disc (de la offset-ul 4)
        uint32_t* next_free = (uint32_t*)(fs_buffer + 4);
        uint32_t used_sectors = *next_free;
        
        print("Dimensiune Sect : 512 bytes"); newline();
        print("Sectoare Ocupate: "); 
        print_number(used_sectors); 
        newline();
        
        print("Spatiu Utilizat : ");
        print_number(used_sectors * 512);
        print(" bytes"); 
        newline();
    } else {
        print("Neformatat / Necunoscut"); newline();
    }
    
    print("---------------------------------"); newline();
}

void fs_format() {
    uint8_t buffer[512];
    
    // 1. Umplem sectorul cu zerouri pentru a șterge toate intrările directoarelor
    for (int i = 0; i < 512; i++) {
        buffer[i] = 0;
    }

    // 2. Rescriem semnătura magică (NAN2)
    buffer[0] = FS_MAGIC[0]; 
    buffer[1] = FS_MAGIC[1]; 
    buffer[2] = FS_MAGIC[2]; 
    buffer[3] = FS_MAGIC[3];

    // 3. Resetăm pointerul pentru sectoare libere (următorul sector disponibil va fi 2)
    uint32_t* next_free = (uint32_t*)(buffer + 4);
    *next_free = 2;

    // 4. Salvăm Sectorul 1 pe disc
    disk_write_sector(1, buffer);

    // 5. Ne asigurăm că shell-ul revine în rădăcină (în caz că eram într-un folder)
    current_dir_sector = 1;

    print("Formatare completa. Discul este acum gol (Sistem NAN2)."); 
    newline();
}