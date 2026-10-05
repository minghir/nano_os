#include <stdint.h>
#include "fs_v3.h"
#include "fs_v3_inode.h"
#include "fs_v3_dir.h"
#include "../syslog.h"

// Funcții externe pentru disk pe hdb (drive 1)
extern void disk_read_sector_drive(uint8_t drive, uint32_t lba, uint8_t* buffer);
extern void disk_write_sector_drive(uint8_t drive, uint32_t lba, const uint8_t* buffer);

extern uint8_t current_drive_id;

// Funcții externe din FAT
extern uint32_t nfs3_alloc_sector(void);
extern uint32_t nfs3_append_sector(uint32_t last_sector);
extern uint32_t nfs3_read_fat_entry(uint32_t disk_sector);
extern void nfs3_free_chain(uint32_t start_sector);

// Funcții externe de afișare din kernel
extern void print(const char* s);
extern void print_number(uint32_t n);
extern void newline();

// Compară două string-uri
static int nfs3_streq(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

/**
 * Caută un nume de fișier/director în lanțul de sectoare al unui director.
 * Returnează indexul inodului asociat sau 0 dacă nu a fost găsit.
 */
uint32_t nfs3_find_in_dir(uint32_t parent_inode_index, const char* name) {
    InodeV3 dir_inode;
    if (!nfs3_read_inode(parent_inode_index, &dir_inode)) return 0xFFFFFFFF;
    
    // Ignorăm verificarea de FLAG_DIR dacă suntem în Root (Inodul 0)
    if (parent_inode_index != 0 && dir_inode.flags != INODE_FLAG_DIR) return 0xFFFFFFFF;

    uint32_t current_sector = dir_inode.first_sector;
    uint32_t entries_per_sector = 512 / sizeof(DirEntryV3);

    while (current_sector != 0xFFFFFFFF && current_sector != 0) {
        uint8_t buffer[512];
        disk_read_sector_drive(current_drive_id, current_sector, buffer);
        DirEntryV3* entries = (DirEntryV3*)buffer;

        for (uint32_t i = 0; i < entries_per_sector; i++) {
            // Asigură-te că folosești funcția ta de comparare (streq sau nfs3_streq)
            if (entries[i].name[0] != '\0' && nfs3_streq(entries[i].name, name)) {
                return entries[i].inode_index;
            }
        }
        current_sector = nfs3_read_fat_entry(current_sector);
    }
    return 0xFFFFFFFF;
}

/**
 * Listează conținutul unui director.
 */
void nfs3_list_dir(uint32_t dir_inode_index) {
    InodeV3 dir_inode;
    if (!nfs3_read_inode(dir_inode_index, &dir_inode)) {
        print("Error: Invalid directory inode.\n");
        return;
    }

    if (dir_inode.flags != INODE_FLAG_DIR || dir_inode.first_sector == 0) {
        print("Error: Not a directory.\n");
        return;
    }

    uint32_t current_sector = dir_inode.first_sector;
    uint32_t entries_per_sector = 512 / sizeof(DirEntryV3);
    int found_any = 0;

    print("NanoFS V3 Directory Contents:\n");

    while (current_sector != 0xFFFFFFFF && current_sector != 0) {
        uint8_t buffer[512];
        disk_read_sector_drive(current_drive_id, current_sector, buffer);

        DirEntryV3* entries = (DirEntryV3*)buffer;
        for (uint32_t i = 0; i < entries_per_sector; i++) {
            if (entries[i].name[0] != '\0') {
                found_any = 1;

                // Citim inodul asociat pentru a afla tipul și dimensiunea
                InodeV3 entry_inode;
                nfs3_read_inode(entries[i].inode_index, &entry_inode);

                if (entry_inode.flags == INODE_FLAG_DIR) {
                    print(" [DIR]  ");
                } else {
                    print(" [FILE] ");
                }

                print(entries[i].name);

                if (entry_inode.flags == INODE_FLAG_FILE) {
                    print(" - ");
                    print_number(entry_inode.size);
                    print(" bytes");
                }
                newline();
            }
        }

        current_sector = nfs3_read_fat_entry(current_sector);
    }

    if (!found_any) {
        print(" (Empty directory)\n");
    }
}

/**
 * Adaugă o intrare nouă (fișier sau director) într-un director părinte.
 */
int nfs3_create_entry(uint32_t parent_inode_index, const char* name, uint8_t flags, uint32_t initial_size) {
    // 1. Verificăm dacă există deja un fișier cu același nume
    // ATENȚIE: Am pus 0xFFFFFFFF (deoarece 0 este acum un Inod valid, Root-ul!)
    if (nfs3_find_in_dir(parent_inode_index, name) != 0xFFFFFFFF) {
        KLOG_ERROR("[NAN3 DIR] Fișierul sau directorul există deja.\n");
        return 0; // Deja există
    }

    // 2. Alocăm un nou inod pentru noul obiect
    uint32_t new_inode_idx = nfs3_alloc_inode();
    if (new_inode_idx == 0) return 0; // Tabela de inoduri plină

    InodeV3 new_inode;
    new_inode.flags = flags;
    new_inode.size = initial_size;
    
    // Alocăm primul sector de date pentru noul obiect (dacă e nevoie)
    uint32_t sectors_needed = initial_size > 0 ? (initial_size + 511) / 512 : 1;
    uint32_t first_sec = nfs3_alloc_sector();
    if (first_sec == 0) {
        nfs3_free_inode(new_inode_idx);
        return 0;
    }
    new_inode.first_sector = first_sec;
    nfs3_write_inode(new_inode_idx, &new_inode);

    // ----------------------------------------------------------------------
    // --- BLOC NOU: Curățăm sectorul și alocăm "." și ".." pentru directoare
    // ----------------------------------------------------------------------
    uint8_t zero_buf[512] = {0}; // Asigură că nu avem gunoi pe disc
    
    if (flags == INODE_FLAG_DIR) {
        DirEntryV3* new_entries = (DirEntryV3*)zero_buf;
        
        // Intrarea "." (pointează spre inodul directorului curent creat)
        new_entries[0].name[0] = '.'; 
        new_entries[0].name[1] = '\0';
        new_entries[0].inode_index = new_inode_idx;

        // Intrarea ".." (pointează spre inodul părintelui)
        new_entries[1].name[0] = '.'; 
        new_entries[1].name[1] = '.'; 
        new_entries[1].name[2] = '\0';
        new_entries[1].inode_index = parent_inode_index;
    }
    
    // Scriem sectorul curățat/inițializat pe hdb (drive 1)
    disk_write_sector_drive(current_drive_id, first_sec, zero_buf);
    // ----------------------------------------------------------------------

    // 3. Căutăm loc liber în lanțul de sectoare al directorului părinte
    InodeV3 parent_inode;
    nfs3_read_inode(parent_inode_index, &parent_inode);

    uint32_t current_sector = parent_inode.first_sector;
    uint32_t last_sector = current_sector;
    uint32_t entries_per_sector = 512 / sizeof(DirEntryV3);
    int placed = 0;

    while (!placed) {
        uint8_t buffer[512];
        disk_read_sector_drive(current_drive_id, current_sector, buffer);

        DirEntryV3* entries = (DirEntryV3*)buffer;
        for (uint32_t i = 0; i < entries_per_sector; i++) {
            if (entries[i].name[0] == '\0') {
                // Am găsit loc liber! Copiem numele (max 63 caractere)
                int j = 0;
                while (name[j] != '\0' && j < 63) {
                    entries[i].name[j] = name[j];
                    j++;
                }
                entries[i].name[j] = '\0';
                entries[i].inode_index = new_inode_idx;

                disk_write_sector_drive(current_drive_id, current_sector, buffer);
                placed = 1;
                break;
            }
        }

        if (placed) break;

        // Dacă sectorul curent e plin, verificăm dacă mai există un sector în lanț
        uint32_t next_sector = nfs3_read_fat_entry(current_sector);
        if (next_sector == 0xFFFFFFFF || next_sector == 0) {
            // Nu mai sunt sectoare în lanț, alocăm unul nou prin FAT!
            uint32_t new_dir_sec = nfs3_append_sector(last_sector);
            if (new_dir_sec == 0) {
                // Cleanup în caz de eșec
                nfs3_free_inode(new_inode_idx);
                nfs3_free_chain(first_sec);
                return 0;
            }
            current_sector = new_dir_sec;
            // Curățăm noul sector cu zerouri
            uint8_t zero_buf2[512] = {0};
            disk_write_sector_drive(current_drive_id, current_sector, zero_buf2);
        } else {
            last_sector = current_sector;
            current_sector = next_sector;
        }
    }

    KLOG_INFO("[NAN3 DIR] Intrare creată cu succes în director.\n");
    return 1;
}

/**
 * Șterge o intrare din director și eliberează resursele (inod și lanț FAT).
 */
int nfs3_delete_entry(uint32_t parent_inode_index, const char* name) {
    InodeV3 parent_inode;
    nfs3_read_inode(parent_inode_index, &parent_inode);

    uint32_t current_sector = parent_inode.first_sector;
    uint32_t entries_per_sector = 512 / sizeof(DirEntryV3);

    while (current_sector != 0xFFFFFFFF && current_sector != 0) {
        uint8_t buffer[512];
        disk_read_sector_drive(current_drive_id, current_sector, buffer);

        DirEntryV3* entries = (DirEntryV3*)buffer;
        for (uint32_t i = 0; i < entries_per_sector; i++) {
            if (entries[i].name[0] != '\0' && nfs3_streq(entries[i].name, name)) {
                uint32_t target_inode_idx = entries[i].inode_index;

                InodeV3 target_inode;
                if (nfs3_read_inode(target_inode_idx, &target_inode)) {
                    
                    // --- PROTECȚIA NOUĂ: Verificăm dacă directorul este gol ---
                    if (target_inode.flags == INODE_FLAG_DIR) {
                        int is_empty = 1;
                        uint32_t check_sec = target_inode.first_sector;
                        
                        // Căutăm prin toate sectoarele directorului țintă
                        while (check_sec != 0xFFFFFFFF && check_sec != 0) {
                            uint8_t check_buf[512];
                            disk_read_sector_drive(current_drive_id, check_sec, check_buf);
                            DirEntryV3* check_entries = (DirEntryV3*)check_buf;
                            
                            for (uint32_t k = 0; k < entries_per_sector; k++) {
                                if (check_entries[k].name[0] != '\0') {
                                    // Dacă găsim ceva care nu e "." și nu e "..", înseamnă că nu e gol!
                                    if (!nfs3_streq(check_entries[k].name, ".") && 
                                        !nfs3_streq(check_entries[k].name, "..")) {
                                        is_empty = 0;
                                        break;
                                    }
                                }
                            }
                            if (!is_empty) break;
                            check_sec = nfs3_read_fat_entry(check_sec);
                        }

                        if (!is_empty) {
                            KLOG_ERROR("[NAN3 DIR] Eroare: Directorul nu este gol!\n");
                            return 0; // Refuzăm ștergerea
                        }
                    }
                    // -----------------------------------------------------------

                    // Dacă este fișier sau dacă este un director cu adevărat gol, îl putem șterge
                    nfs3_free_chain(target_inode.first_sector);
                    nfs3_free_inode(target_inode_idx);
                }

                // Inovalidăm intrarea din directorul părinte
                entries[i].name[0] = '\0';
                entries[i].inode_index = 0;

                disk_write_sector_drive(current_drive_id, current_sector, buffer);
                KLOG_INFO("[NAN3 DIR] Intrare ștearsă cu succes.\n");
                return 1;
            }
        }

        current_sector = nfs3_read_fat_entry(current_sector);
    }

    return 0;
}