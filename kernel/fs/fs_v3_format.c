#include <stdint.h>
#include "fs_v3.h"
#include "fs_v3_inode.h"
#include "../syslog.h"

// Funcții externe de disk I/O
extern void disk_read_sector_drive(uint8_t drive, uint32_t lba, uint8_t* buffer);
extern void disk_write_sector_drive(uint8_t drive, uint32_t lba, const uint8_t* buffer);

// Declarații externe pentru funcțiile FAT
extern uint32_t nfs3_alloc_sector(void);
extern void nfs3_write_fat_entry(uint32_t disk_sector, uint32_t value);

/**
 * Formatează discul conform specificației NanoFS V3.
 * Presupunem un disc de dimensiune standard (de ex. 2048 sectoare = 1 MB, sau preluat din config).
 */
void nfs3_format(void) {
    uint8_t buffer[512];
    
    for (int i = 0; i < 512; i++) {
        buffer[i] = 0;
    }

    // 50MB / 512 = 102,400 sectoare pentru hdb_v3.img
    uint32_t total_sectors = 102400; 
    
    SuperblockV3* sb = (SuperblockV3*)buffer;
    sb->magic[0] = NFS3_MAGIC[0];
    sb->magic[1] = NFS3_MAGIC[1];
    sb->magic[2] = NFS3_MAGIC[2];
    sb->magic[3] = NFS3_MAGIC[3];
    sb->total_sectors = total_sectors;
    sb->fat_start_sector = 2;
    sb->fat_sectors = 200;       // Suficient pentru a acoperi mii de sectoare FAT
    sb->inode_start = 202;
    sb->inode_count = 1000;      // 1000 inoduri
    sb->data_start = 1226;       // Zona de date începe după sistem

    // SCRIEM PE DRIVE 1 (HDB) EXCLUSIV!
    disk_write_sector_drive(1, 1, buffer);
    KLOG_INFO("[NAN3] Superblock scris în Sectorul 1 pe hdb.\n");

    // Inițializăm Tabela FAT pe hdb
    for (uint32_t s = sb->fat_start_sector; s < sb->fat_start_sector + sb->fat_sectors; s++) {
        for (int i = 0; i < 512; i++) buffer[i] = 0;
        disk_write_sector_drive(1, s, buffer);
    }

    // Marcăm sectoarele de sistem ca FAT_BAD
    for (uint32_t s = 0; s < sb->data_start; s++) {
        nfs3_write_fat_entry(s, FAT_BAD);
    }
    KLOG_INFO("[NAN3] Tabela FAT inițializată.\n");

    // Inițializăm zona de Inoduri pe hdb
    uint32_t inode_sectors = (sb->inode_count * sizeof(InodeV3) + 511) / 512;
    for (uint32_t s = sb->inode_start; s < sb->inode_start + inode_sectors; s++) {
        for (int i = 0; i < 512; i++) buffer[i] = 0;
        disk_write_sector_drive(1, s, buffer);
    }
    KLOG_INFO("[NAN3] Zona de inoduri inițializată.\n");

    // Creăm Inodul Root (Index 0)
    InodeV3 root_inode;
    root_inode.flags = INODE_FLAG_DIR;
    root_inode.size = 0;
    root_inode.first_sector = nfs3_alloc_sector();
    
    nfs3_write_inode(0, &root_inode);

    for (int i = 0; i < 512; i++) buffer[i] = 0;
    disk_write_sector_drive(1, root_inode.first_sector, buffer);

    KLOG_INFO("[NAN3] Formatare completă cu succes pe hdb! Directorul Root (Inod 0) creat.\n");
}

/**
 * Funcția de inițializare a sistemului de fișiere V3 la boot.
 */
void nfs3_init(void) {
    uint8_t buffer[512];
    disk_read_sector_drive(1, 1, buffer);

    SuperblockV3* sb = (SuperblockV3*)buffer;

    // Verificăm dacă discul are semnătura "NAN3"
    if (sb->magic[0] == NFS3_MAGIC[0] &&
        sb->magic[1] == NFS3_MAGIC[1] &&
        sb->magic[2] == NFS3_MAGIC[2] &&
        sb->magic[3] == NFS3_MAGIC[3]) {
        KLOG_INFO("[NAN3] Sistemul de fișiere NFS3 detectat și montat cu succes.\n");
    } else {
        KLOG_WARNING("[NAN3] Disc neformatat sau versiune necunoscută. Se rulează formatarea automată...\n");
        nfs3_format();
    }
}