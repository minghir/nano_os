#ifndef NANOFS_V3_H
#define NANOFS_V3_H

#include <stdint.h>

#define NFS3_MAGIC "NAN3"

#define FAT_FREE     0x00000000
#define FAT_EOF      0xFFFFFFFF
#define FAT_BAD      0xFFFFFFFE

#define INODE_FLAG_UNUSED 0x00
#define INODE_FLAG_FILE   0x01
#define INODE_FLAG_DIR    0x02

// Superblock (Stocat în Sectorul 1)
typedef struct {
    char magic[4];             // "NAN3"
    uint32_t total_sectors;    // Dimensiunea totală a discului
    uint32_t fat_start_sector; // Unde începe tabela FAT
    uint32_t fat_sectors;      // Câte sectoare ocupă tabela FAT
    uint32_t inode_start;      // Unde începe zona de inoduri
    uint32_t inode_count;      // Câte inoduri totale avem
    uint32_t data_start;       // Unde începe zona de date brute
} __attribute__((packed)) SuperblockV3;

// Inodul (Metadatele fișierului/directorului - Multi-User & Timestamps)
typedef struct {
    uint32_t size;          // Dimensiunea reală în octeți
    uint32_t first_sector;  // Primul sector din lanțul FAT
    uint8_t  flags;         // 0x01 = File, 0x02 = Dir
    uint8_t  reserved[3];   // Aliniere
    
    // Multi-User Extensions
    uint32_t uid;           // ID-ul utilizatorului proprietar
    uint32_t gid;           // ID-ul grupului proprietar
    uint16_t permissions;   // Biții de permisiune (ex: 0755 sau 0644)
    uint16_t reserved_perm; // Aliniere suplimentară pentru aliniere la multipli de 4 octeți

    // Timestamps (Timp Unix timestamp în secunde sau un format intern de date/timp)
    uint32_t create_time;   // Momentul creării fișierului
    uint32_t modify_time;   // Ultima modificare a conținutului
    
} __attribute__((packed)) InodeV3;

// Intrare în Director cu Nume Extins la 64 de caractere
typedef struct {
    char name[64];             // Nume de până la 63 caractere + '\0'
    uint32_t inode_index;      // Indexul inodului asociat
} __attribute__((packed)) DirEntryV3; // 68 octeți

void nfs3_init(void);
void nfs3_format(void);
void read_superblock(SuperblockV3* sb);

int nfs3_get_file_at_index(int index, char* buffer, uint32_t max_len);

#endif