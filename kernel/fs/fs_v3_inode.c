#include <stdint.h>
#include "fs_v3.h"
#include "fs_v3_inode.h"
#include "../syslog.h"
#include "../ata.h"

// Funcții externe de disk I/O
extern void disk_read_sector_drive(uint8_t drive, uint32_t lba, uint8_t* buffer);
extern void disk_write_sector_drive(uint8_t drive, uint32_t lba, const uint8_t* buffer);

// Citim Superblock-ul
/*
void read_superblock(SuperblockV3* sb) {
    uint8_t buffer[512];
    disk_read_sector_drive(1, 1, buffer);
    for (uint32_t i = 0; i < sizeof(SuperblockV3); i++) {
        ((uint8_t*)sb)[i] = buffer[i];
    }
}
*/

/**
 * Citește un inod după indexul său global de pe disc.
 */
int nfs3_read_inode(uint32_t inode_index, InodeV3* out_inode) {
	
	// Dacă ni se cere să citim inodul -1 (gol/inexistent), returnăm 0 fără să printăm eroare
    if (inode_index == 0xFFFFFFFF) return 0;
	
    SuperblockV3 sb;
    read_superblock(&sb);

    if (inode_index >= sb.inode_count) {
        KLOG_ERROR("[NAN3 INODE ERROR] Index de inod invalid (depășește limita).\n");
        return 0;
    }

    uint32_t inodes_per_sector = 512 / sizeof(InodeV3); // ~42
    uint32_t sector_offset = inode_index / inodes_per_sector;
    uint32_t entry_index = inode_index % inodes_per_sector;

    uint32_t target_sector = sb.inode_start + sector_offset;

    uint8_t buffer[512];
    disk_read_sector_drive(1, target_sector, buffer);

    InodeV3* inodes = (InodeV3*)buffer;
    *out_inode = inodes[entry_index];

    return 1;
}

/**
 * Scrie un inod pe disc la indexul specificat.
 */
int nfs3_write_inode(uint32_t inode_index, const InodeV3* inode) {
    SuperblockV3 sb;
    read_superblock(&sb);

    if (inode_index >= sb.inode_count) {
        KLOG_ERROR("[NAN3 INODE ERROR] Încercare de scriere pe un inod invalid.");
        return 0;
    }

    uint32_t inodes_per_sector = 512 / sizeof(InodeV3);
    uint32_t sector_offset = inode_index / inodes_per_sector;
    uint32_t entry_index = inode_index % inodes_per_sector;

    uint32_t target_sector = sb.inode_start + sector_offset;

    uint8_t buffer[512];
    disk_read_sector_drive(1, target_sector, buffer);

    InodeV3* inodes = (InodeV3*)buffer;
    inodes[entry_index] = *inode;

    disk_write_sector_drive(1, target_sector, buffer);
    return 1;
}

/**
 * Caută primul inod neutilizat (UNUSED) și îl rezervă.
 * Returnează indexul inodului alocat sau 0 în caz de eșec (tabel de inoduri plin).
 */
uint32_t nfs3_alloc_inode(void) {
    SuperblockV3 sb;
    read_superblock(&sb);

    uint32_t inodes_per_sector = 512 / sizeof(InodeV3);
    uint32_t total_inode_sectors = (sb.inode_count + inodes_per_sector - 1) / inodes_per_sector;

    // Sărim peste inodul 0 (considerat rezervat/root)
    for (uint32_t s = 0; s < total_inode_sectors; s++) {
        uint8_t buffer[512];
        disk_read_sector_drive(1, sb.inode_start + s, buffer);
        InodeV3* inodes = (InodeV3*)buffer;

        for (uint32_t i = 0; i < inodes_per_sector; i++) {
            uint32_t global_index = s * inodes_per_sector + i;
            if (global_index >= sb.inode_count) break;

            if (inodes[i].flags == INODE_FLAG_UNUSED) {
                // Găsit! Îl marcăm provizoriu ca fișier
                inodes[i].flags = INODE_FLAG_FILE;
                inodes[i].size = 0;
                inodes[i].first_sector = 0;

                disk_write_sector_drive(1, sb.inode_start + s, buffer);
                KLOG_DEBUG("[NAN3 INODE] Inod nou alocat cu succes.\n");
                return global_index;
            }
        }
    }

    KLOG_ERROR("[NAN3 INODE ERROR] Tabela de inoduri este plină!\n");
    return 0;
}

/**
 * Eliberează un inod, marcându-l caUNUSED.
 */
void nfs3_free_inode(uint32_t inode_index) {
    InodeV3 inode;
    if (nfs3_read_inode(inode_index, &inode)) {
        inode.flags = INODE_FLAG_UNUSED;
        inode.size = 0;
        inode.first_sector = 0;
        nfs3_write_inode(inode_index, &inode);
        KLOG_INFO("[NAN3 INODE] Inod eliberat.\n");
    }
}