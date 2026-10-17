#ifndef EXT2_H
#define EXT2_H

#include <stdint.h>

#define EXT2_SUPER_MAGIC 0xEF53

// Superblock-ul EXT2 se află mereu la offset-ul de 1024 bytes de la începutul partiției
typedef struct {
    uint32_t s_inodes_count;      // Numărul total de inode-uri
    uint32_t s_blocks_count;      // Numărul total de blocuri
    uint32_t s_r_blocks_count;    // Blocuri rezervate
    uint32_t s_free_blocks_count; // Blocuri libere
    uint32_t s_free_inodes_count; // Inode-uri libere
    uint32_t s_first_data_block;  // Primul bloc de date (0 pentru 1KB blocks, 1 pentru >1KB)
    uint32_t s_log_block_size;    // Dimensiunea blocului
    uint32_t s_log_frag_size;     // Dimensiunea fragmentului
    uint32_t s_blocks_per_group;  // Blocuri per grup
    uint32_t s_frags_per_group;   // Fragmente per grup
    uint32_t s_inodes_per_group;  // Inode-uri per grup
    uint32_t s_mtime;             // Timpul ultimei montări
    uint32_t s_wtime;             // Timpul ultimei scrieri
    uint16_t s_mnt_count;         // Numărul de montări de la ultimul fsck
    uint16_t s_max_mnt_count;     // Numărul maxim de montări înainte de fsck
    uint16_t s_magic;             // Semnătura magică (0xEF53)
    uint16_t s_state;             // Starea sistemului de fișiere
    uint16_t s_errors;            // Ce face la erori
    uint16_t s_minor_rev_level;   // Revizie minoră
    uint32_t s_lastcheck;         // Timpul ultimului fsck
    uint32_t s_checkinterval;     // Intervalul între fsck-uri
    uint32_t s_creator_os;        // OS-ul creator
    uint32_t s_rev_level;         // Nivelul de revizie (0 = V1, 1 = dinamic)
    
    // --- Câmpurile extinse adăugate pentru partiții moderne ---
    uint16_t s_def_resuid;        // Default UID
    uint16_t s_def_resgid;        // Default GID
    uint32_t s_first_ino;         // Primul Inode non-rezervat
    uint16_t s_inode_size;        // DIMENSIUNEA REALĂ A INODULUI (Aici era secretul!)
    uint16_t s_block_group_nr;    // Numărul grupului de blocuri din care face parte acest superblock
    uint32_t s_feature_compat;    // Features compatibile
    uint32_t s_feature_incompat;  // Features incompatibile
    uint32_t s_feature_ro_compat; // Features Read-Only
    
    // Umplem structura până la 1024 bytes pentru a ne asigura că bucla din init() o copiază pe toată
    uint8_t  s_padding[920]; 
} __attribute__((packed)) ext2_superblock_t;

// Inodul EXT2 (descrie un fișier sau director)
typedef struct {
    uint16_t i_mode;        // Tip și permisiuni
    uint16_t i_uid;         // ID-ul utilizatorului
    uint32_t i_size;        // Dimensiunea fișierului în bytes
    uint32_t i_atime;       // Ultima accesare
    uint32_t i_ctime;       // Creare
    uint32_t i_mtime;       // Modificare
    uint32_t i_dtime;       // Ștergere
    uint16_t i_gid;         // ID-ul grupului
    uint16_t i_links_count; // Număr de hard link-uri
    uint32_t i_blocks;      // Sectoare de disc ocupate (pe 512 bytes, nu blocuri EXT2!)
    uint32_t i_flags;       // Flag-uri
    uint32_t i_osd1;        // OS dependent 1
    uint32_t i_block[15];   // 12 pointeri directi, 1 indirect, 1 dublu-indirect, 1 triplu-indirect
    uint32_t i_generation;  // File version (NFS)
    uint32_t i_file_acl;    // File ACL
    uint32_t i_dir_acl;     // Directory ACL
    uint32_t i_fader;       // Fragment address
    uint8_t  osd2[12];      // OS dependent 2
} __attribute__((packed)) ext2_inode_t;

// Structura pentru o intrare într-un director (Directory Entry)
typedef struct {
    uint32_t inode;     // Numărul inode-ului
    uint16_t rec_len;   // Lungimea totală a acestei înregistrări
    uint8_t  name_len;  // Lungimea numelui fișierului
    uint8_t  file_type; // Tipul fișierului (1=fișier, 2=director, etc.)
    char     name[];    // Numele (flexibil, de lungime name_len)
} __attribute__((packed)) ext2_dir_entry_t;

#endif