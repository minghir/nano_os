#include "fs.h"
#include "ext2.h"
#include "../syslog.h"
#include "../string.h"
#include "../memory.h" // Pentru malloc/free

// Tipurile de fișiere EXT2
#define EXT2_FT_UNKNOWN  0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR      2

// Structura pentru Block Group Descriptor (necesară pentru a găsi Inodurile)
typedef struct {
    uint32_t bg_block_bitmap;      // Blocul care ține harta blocurilor libere
    uint32_t bg_inode_bitmap;      // Blocul care ține harta inodurilor libere
    uint32_t bg_inode_table;       // Blocul de START al tabelei de inoduri pentru acest grup
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint32_t bg_reserved[3];
} __attribute__((packed)) ext2_block_group_desc_t;

// Legăturile către mediul sistemului tău de operare
extern uint8_t current_drive_id;
extern void disk_read_sector_drive(uint8_t drive, uint32_t lba, uint8_t* buffer);

// Variabila globală care ține Superblock-ul EXT2 (încărcat la boot/init)
ext2_superblock_t ext2_superblock;

// O funcție simplă de comparație șiruri
static int streq(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}


// -------------------------------------------------------------------------
// Citește un bloc întreg EXT2 de pe disc.
// Un bloc EXT2 poate avea 1024, 2048 sau 4096 octeți (adică 2, 4 sau 8 sectoare de disc).
// -------------------------------------------------------------------------
int ext2_read_block(uint32_t block_num, uint8_t* buffer) {
    if (block_num == 0) return 0;

    uint32_t block_size = 1024 << ext2_superblock.s_log_block_size;
    uint32_t sectors_per_block = block_size / 512;
    uint32_t lba = block_num * sectors_per_block;

    // Citim sector cu sector până umplem blocul
    for (uint32_t i = 0; i < sectors_per_block; i++) {
        disk_read_sector_drive(current_drive_id, lba + i, buffer + (i * 512));
    }
    return 1;
}

// -------------------------------------------------------------------------
// Găsește și citește un Inod pe baza numărului său (ex: 2 pentru Root).
// Funcția localizează Block Group-ul, citește descriptorul și găsește tabela.
// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
// Găsește și citește un Inod pe baza numărului său (ex: 2 pentru Root).
// -------------------------------------------------------------------------
int ext2_read_inode(uint32_t inode_num, ext2_inode_t* out_inode) {
    if (inode_num == 0) return 0;
    
    uint32_t block_size = 1024 << ext2_superblock.s_log_block_size;
    
    // Dacă e un sistem modern (Rev >= 1), folosim s_inode_size (ex: 256). Altfel, clasicul 128.
    uint32_t inode_size = (ext2_superblock.s_rev_level >= 1) ? ext2_superblock.s_inode_size : 128;
    
    uint32_t group = (inode_num - 1) / ext2_superblock.s_inodes_per_group;
    uint32_t index = (inode_num - 1) % ext2_superblock.s_inodes_per_group;
    
    uint32_t bgdt_block = ext2_superblock.s_first_data_block + 1;
    
    uint32_t desc_size = sizeof(ext2_block_group_desc_t);
    uint32_t bg_block_offset = (group * desc_size) / block_size;
    uint32_t bg_offset_in_block = (group * desc_size) % block_size;
    
    uint8_t* temp_buf = (uint8_t*)malloc(block_size);
    if (!temp_buf) return 0;
    
    ext2_read_block(bgdt_block + bg_block_offset, temp_buf);
    
    ext2_block_group_desc_t bgd;
    char* src = (char*)(temp_buf + bg_offset_in_block);
    char* dst = (char*)&bgd;
    for (uint32_t i = 0; i < desc_size; i++) dst[i] = src[i];
    
    uint32_t inode_table_block = bgd.bg_inode_table;
    
    // AICI E CORECȚIA: Înmulțim indexul cu mărimea REALA de pe disc (ex: 256), nu cu structura noastră
    uint32_t inode_block_offset = (index * inode_size) / block_size;
    uint32_t inode_offset_in_block = (index * inode_size) % block_size;
    
    ext2_read_block(inode_table_block + inode_block_offset, temp_buf);
    
    src = (char*)(temp_buf + inode_offset_in_block);
    dst = (char*)out_inode;
    
    // Deși inodul are 256 octeți pe disc, structura noastră de bază are nevoie doar de primii 128 octeți
    // Așa că transferăm strict cât încape în ext2_inode_t
    for (uint32_t i = 0; i < sizeof(ext2_inode_t); i++) dst[i] = src[i];
    
    free(temp_buf);
    return 1;
}


// --- Implementarea funcțiilor pentru FileSystemInterface ---

void ext2_init(void) {
    // Superblock-ul EXT2 se află MEREU fix la octetul 1024 de la începutul partiției.
    // Un sector are 512 bytes, deci Superblock-ul începe în LBA 2 și se termină în LBA 3.
    uint8_t buf[1024]; 
    disk_read_sector_drive(current_drive_id, 2, buf);
    disk_read_sector_drive(current_drive_id, 3, buf + 512);

    char* dst = (char*)&ext2_superblock;
    for (uint32_t i = 0; i < sizeof(ext2_superblock_t); i++) {
        dst[i] = buf[i];
    }

    if (ext2_superblock.s_magic == EXT2_SUPER_MAGIC) {
        KLOG_INFO("[EXT2] Superblock valid! Sistem de fisiere gata.\n");
    } else {
        KLOG_ERROR("[EXT2] EROARE: Magic number invalid! Acest disc nu este formatat EXT2.\n");
    }
}

static int ext2_get_stats(uint32_t* total_sectors, uint32_t* free_sectors) {
    // Opțional: returnează statistici preluate din superblock
    return -1; 
}

static void ext2_list_files(const char* path) {
    // 1. În EXT2, Inodul Rădăcină (Root) este mereu 2.
    // Până când implementezi rutarea prin subdirectoare, vom lista root-ul partiției.
    uint32_t target_inode_num = 2; 

    // 2. Citim inodul de pe disc
    ext2_inode_t dir_inode;
    if (!ext2_read_inode(target_inode_num, &dir_inode)) {
        print("[EXT2 EROARE] Nu s-a putut citi inodul directorului.\n");
        return;
    }

    // Verificăm dacă inodul aparține într-adevăr unui director (0x4000 = masca pentru director)
    if ((dir_inode.i_mode & 0xF000) != 0x4000) {
        print("[EXT2] Eroare: Tinta nu este un director!\n");
        return;
    }

    print("EXT2 Directory Contents:\n");

    // Calculăm dimensiunea unui bloc. În EXT2: Block Size = 1024 << s_log_block_size
    uint32_t block_size = 1024 << ext2_superblock.s_log_block_size; 
    
    // Alocăm un buffer pentru a citi un bloc întreg
    uint8_t* block_buffer = (uint8_t*)malloc(block_size);
    if (!block_buffer) {
        print("[EXT2] Eroare alocare memorie.\n");
        return;
    }

    uint32_t bytes_read = 0;

    // 3. Parcurgem blocurile directe ale inodului (primele 12 blocuri)
    for (int i = 0; i < 12 && bytes_read < dir_inode.i_size; i++) {
        uint32_t block_num = dir_inode.i_block[i];
        
        if (block_num == 0) break; // Am ajuns la capătul blocurilor alocate

        // Citim blocul fizic de pe disc în buffer-ul nostru
        ext2_read_block(block_num, block_buffer);

        uint32_t offset = 0;

        // 4. Parcurgem lista înlănțuită de intrări din interiorul acestui bloc
        while (offset < block_size && bytes_read < dir_inode.i_size) {
            ext2_dir_entry_t* entry = (ext2_dir_entry_t*)(block_buffer + offset);

            // Dacă rec_len e 0, înseamnă că sistemul de fișiere e corupt (previne bucla infinită)
            if (entry->rec_len == 0) break; 

            // Dacă inodul e diferit de 0, intrarea este validă (nu a fost ștearsă)
            if (entry->inode != 0) {
                char name[256];
                int name_l = entry->name_len;
                if (name_l > 255) name_l = 255;
                
                // Copiem numele caracter cu caracter (în EXT2, numele NU se termină cu '\0')
                for (int n = 0; n < name_l; n++) {
                    name[n] = entry->name[n];
                }
                name[name_l] = '\0';

                // Ignorăm folderul curent "." și părintele ".." pentru un afișaj mai curat în consolă
                if (!streq(name, ".") && !streq(name, "..")) {
                    char output[128];
                    if (entry->file_type == EXT2_FT_DIR) {
                        snprintf(output, sizeof(output), " [DIR]  %s\n", name);
                    } else {
                        snprintf(output, sizeof(output), " [FILE] %s\n", name);
                    }
                    print(output);
                }
            }

            // Sărim la următoarea intrare adunând lungimea intrării curente
            offset += entry->rec_len;
            bytes_read += entry->rec_len;
        }
    }

    free(block_buffer);
}


// Caută un nume de fișier în directorul Root (Inode 2) și returnează ID-ul inodului
static uint32_t ext2_find_in_root(const char* target_name) {
    if (target_name[0] == '/') target_name++; // Ignorăm slash-ul inițial

    ext2_inode_t dir_inode;
    if (!ext2_read_inode(2, &dir_inode)) return 0; // 2 este Root-ul

    uint32_t block_size = 1024 << ext2_superblock.s_log_block_size;
    uint8_t* block_buffer = (uint8_t*)malloc(block_size);
    if (!block_buffer) return 0;

    uint32_t bytes_read = 0;
    // Căutăm doar în blocurile directe (suficient pentru rădăcina unui disc simplu)
    for (int i = 0; i < 12 && bytes_read < dir_inode.i_size; i++) {
        uint32_t block_num = dir_inode.i_block[i];
        if (block_num == 0) break;

        ext2_read_block(block_num, block_buffer);
        uint32_t offset = 0;

        while (offset < block_size && bytes_read < dir_inode.i_size) {
            ext2_dir_entry_t* entry = (ext2_dir_entry_t*)(block_buffer + offset);
            if (entry->rec_len == 0) break;

            if (entry->inode != 0) {
                char name[256];
                int name_l = entry->name_len;
                if (name_l > 255) name_l = 255;
                for (int n = 0; n < name_l; n++) name[n] = entry->name[n];
                name[name_l] = '\0';

                if (streq(name, target_name)) {
                    uint32_t found_inode = entry->inode;
                    free(block_buffer);
                    return found_inode; // Am găsit fișierul!
                }
            }
            offset += entry->rec_len;
            bytes_read += entry->rec_len;
        }
    }
    free(block_buffer);
    return 0; // Nu a fost găsit
}


static uint32_t ext2_get_physical_block(ext2_inode_t* inode, uint32_t logical_block) {
    uint32_t block_size = 1024 << ext2_superblock.s_log_block_size;
    uint32_t ptrs_per_block = block_size / 4; // Câți pointeri încap într-un bloc (ex: 256)

    // 1. BLOCURI DIRECTE (0 - 11)
    if (logical_block < 12) {
        return inode->i_block[logical_block];
    }
    logical_block -= 12;

    // 2. SINGLY INDIRECT BLOCK (Până la +256 blocuri)
    if (logical_block < ptrs_per_block) {
        uint32_t singly_block = inode->i_block[12];
        if (singly_block == 0) return 0;

        uint32_t* table = (uint32_t*)malloc(block_size);
        ext2_read_block(singly_block, (uint8_t*)table);
        uint32_t phys_block = table[logical_block];
        
        free(table);
        return phys_block;
    }
    logical_block -= ptrs_per_block;

    // 3. DOUBLY INDIRECT BLOCK (Până la +65536 blocuri, perfect pentru DOOM)
    if (logical_block < ptrs_per_block * ptrs_per_block) {
        uint32_t doubly_block = inode->i_block[13];
        if (doubly_block == 0) return 0;

        uint32_t table_idx = logical_block / ptrs_per_block;
        uint32_t inner_idx = logical_block % ptrs_per_block;

        uint32_t* table = (uint32_t*)malloc(block_size);
        
        // Citim blocul dublu-indirect (tabel de tabele)
        ext2_read_block(doubly_block, (uint8_t*)table);
        uint32_t singly_block = table[table_idx];

        if (singly_block == 0) {
            free(table);
            return 0;
        }

        // Citim blocul simplu-indirect (tabelul final cu date)
        ext2_read_block(singly_block, (uint8_t*)table);
        uint32_t phys_block = table[inner_idx];
        
        free(table);
        return phys_block;
    }

    return 0; // Nu suportăm încă triply-indirect (fișiere de zeci de GB)
}


static int ext2_read_file(const char* name, uint8_t* buffer, uint32_t max_size) {
    if (!name || !buffer) return 0;

    // 1. Găsim inodul fișierului
    uint32_t inode_num = ext2_find_in_root(name);
    if (inode_num == 0) {
        print("[EXT2] Fisierul nu a fost gasit.\n");
        return 0;
    }

    // 2. Citim structura inodului
    ext2_inode_t file_inode;
    if (!ext2_read_inode(inode_num, &file_inode)) return 0;

    // Verificăm dacă e fișier normal (0x8000 = Regular File)
    if ((file_inode.i_mode & 0xF000) != 0x8000) {
        print("[EXT2] Calea nu este un fisier valid!\n");
        return 0;
    }

    uint32_t block_size = 1024 << ext2_superblock.s_log_block_size;
    uint32_t bytes_to_read = file_inode.i_size;
    
    // Asigurăm protecție să nu trecem de memoria alocată de shell/joc
    if (bytes_to_read > max_size) bytes_to_read = max_size;

    uint32_t bytes_read = 0;
    uint32_t logical_block = 0;
    
    uint8_t* temp_block = (uint8_t*)malloc(block_size);
    if (!temp_block) return 0;

    // 3. Citim bloc cu bloc până copiem tot fișierul
    while (bytes_read < bytes_to_read) {
        uint32_t phys_block = ext2_get_physical_block(&file_inode, logical_block);
        
        uint32_t chunk_size = block_size;
        if (bytes_read + chunk_size > bytes_to_read) {
            chunk_size = bytes_to_read - bytes_read; // Ultimul bloc ar putea fi umplut parțial
        }

        if (phys_block == 0) {
            // EXT2 suportă "Sparse files" (blocuri goale nescrie pe disc). Le umplem cu zero.
            for (uint32_t i = 0; i < chunk_size; i++) buffer[bytes_read + i] = 0;
        } else {
            // Citim blocul fizic curat
            ext2_read_block(phys_block, temp_block);
            for (uint32_t i = 0; i < chunk_size; i++) buffer[bytes_read + i] = temp_block[i];
        }

        bytes_read += chunk_size;
        logical_block++;
    }

    free(temp_block);
    return bytes_read; // Returnăm câți octeți au fost încărcați în memorie
}

static int ext2_cd(const char* name) {
    // Schimbă directorul curent în interiorul structurii EXT2
    return -1;
}

static void ext2_get_current_path(char* buffer, uint32_t max_len) {
    // Returnează calea curentă (ex: "/")
    if (max_len > 0) buffer[0] = '/';
    if (max_len > 1) buffer[1] = '\0';
}

// Funcții neimplementate momentan (Read-Only)
static int ext2_create_file(const char* name, uint32_t size) { return -1; }
static int ext2_delete_file(const char* name) { return -1; }
static int ext2_write_file(const char* name, const uint8_t* data, uint32_t size) { return -1; }
static int ext2_mkdir(const char* name) { return -1; }
static int ext2_rmdir(const char* name) { return -1; }
static void ext2_fdisk(void) {}
static void ext2_format(void) {}
static int ext2_get_file_at_index(int index, char* buffer, uint32_t max_len) { return -1; }
static int ext2_append_file(const char* path, const char* data) { return -1; }

// --- EXPORTAREA DRIVERULUI ---
FileSystemInterface ext2_driver = {
    .name = "ext2",
    .init = ext2_init,
    .get_stats = ext2_get_stats,
    .list_files = ext2_list_files,
    .create_file = ext2_create_file,
    .delete_file = ext2_delete_file,
    .write_file = ext2_write_file,
    .read_file = ext2_read_file,
    .mkdir = ext2_mkdir,
    .rmdir = ext2_rmdir,
    .cd = ext2_cd,
    .get_current_path = ext2_get_current_path,
    .fdisk = ext2_fdisk,
    .format = ext2_format,
    .get_file_at_index = ext2_get_file_at_index,
    .append_file = ext2_append_file
};