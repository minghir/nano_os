#include <stdint.h>
#include "fs_v3.h"
#include "../string.h"
#include "../syslog.h" // Includem syslog pentru macro-urile KLOG_...

// Funcții externe de disk I/O din kernel
extern void disk_read_sector_drive(uint8_t drive, uint32_t lba, uint8_t* buffer);
extern void disk_write_sector_drive(uint8_t drive, uint32_t lba, const uint8_t* buffer);

// Citim Superblock-ul din Sectorul 1 pentru a afla unde se află tabela FAT
void read_superblock(SuperblockV3* sb) {
    uint8_t buffer[512];
    disk_read_sector_drive(1, 1, buffer);
    for (uint32_t i = 0; i < sizeof(SuperblockV3); i++) {
        ((uint8_t*)sb)[i] = buffer[i];
    }
}

/**
 * Citește valoarea dintr-o intrare FAT pentru un anumit sector de disc.
 */
uint32_t nfs3_read_fat_entry(uint32_t disk_sector) {
    SuperblockV3 sb;
    read_superblock(&sb);

    uint32_t fat_sector_offset = disk_sector / 128;
    uint32_t entry_index = disk_sector % 128;
    uint32_t target_sector = sb.fat_start_sector + fat_sector_offset;

    uint8_t buffer[512];
    disk_read_sector_drive(1,target_sector, buffer);

    uint32_t* entries = (uint32_t*)buffer;
    return entries[entry_index];
}

/**
 * Scrie o valoare într-o intrare FAT.
 */
void nfs3_write_fat_entry(uint32_t disk_sector, uint32_t value) {
    SuperblockV3 sb;
    read_superblock(&sb);

    uint32_t fat_sector_offset = disk_sector / 128;
    uint32_t entry_index = disk_sector % 128;
    uint32_t target_sector = sb.fat_start_sector + fat_sector_offset;

    uint8_t buffer[512];
    disk_read_sector_drive(1,target_sector, buffer);

    uint32_t* entries = (uint32_t*)buffer;
    entries[entry_index] = value;

    disk_write_sector_drive(1, target_sector, buffer);
}

/**
 * Caută primul sector liber pe disc și îl alocă.
 */
uint32_t nfs3_alloc_sector(void) {
    SuperblockV3 sb;
    read_superblock(&sb);

    for (uint32_t s = sb.data_start; s < sb.total_sectors; s++) {
        if (nfs3_read_fat_entry(s) == FAT_FREE) {
            nfs3_write_fat_entry(s, FAT_EOF);
            KLOG_DEBUG("[NAN3 FAT] Sector alocat cu succes.");
            return s;
        }
    }

    KLOG_ERROR("[NAN3 FAT ERROR] Discul este plin! Nu mai există sectoare libere.");
    return 0; // Disk full!
}

/**
 * Eliberează un lanț întreg de sectoare (la ștergerea fișierelor).
 */
void nfs3_free_chain(uint32_t start_sector) {
    uint32_t current = start_sector;
    uint32_t count = 0;

    while (current != FAT_EOF && current != FAT_FREE && current != 0) {
        uint32_t next = nfs3_read_fat_entry(current);
        nfs3_write_fat_entry(current, FAT_FREE);
        count++;
        current = next;
    }
    
    KLOG_INFO("[NAN3 FAT] Lanț de sectoare eliberat.");
}

/**
 * Adaugă un sector nou la sfârșitul unui lanț existent (creștere dinamică).
 */
uint32_t nfs3_append_sector(uint32_t last_sector) {
    uint32_t new_sector = nfs3_alloc_sector();
    if (new_sector == 0) {
        KLOG_ERROR("[NAN3 FAT ERROR] Nu s-a putut atașa un sector nou: disc plin.");
        return 0; 
    }

    nfs3_write_fat_entry(last_sector, new_sector);
    nfs3_write_fat_entry(new_sector, FAT_EOF);

    KLOG_DEBUG("[NAN3 FAT] Sector nou atașat la lanțul existent.");
    return new_sector;
}