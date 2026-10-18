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


// Variabila globală în driver care reține unde ne aflăm pe EXT2 (implicit 2 = Root)
static uint32_t ext2_current_inode = 2; 
static char ext2_cwd_path[256] = "/";

// Legăturile către mediul sistemului tău de operare
extern uint8_t current_drive_id;
extern void disk_read_sector_drive(uint8_t drive, uint32_t lba, uint8_t* buffer);
extern void disk_write_sector_drive(uint8_t drive, uint32_t lba, const uint8_t* buffer);
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


// -------------------------------------------------------------------------
// SCRIE UN BLOC EXT2 PE DISC
// -------------------------------------------------------------------------
int ext2_write_block(uint32_t block_num, const uint8_t* buffer) {
    if (block_num == 0) return 0;

    uint32_t block_size = 1024 << ext2_superblock.s_log_block_size;
    uint32_t sectors_per_block = block_size / 512;
    uint32_t lba = block_num * sectors_per_block;

    for (uint32_t i = 0; i < sectors_per_block; i++) {
        disk_write_sector_drive(current_drive_id, lba + i, buffer + (i * 512));
    }
    return 1;
}

// -------------------------------------------------------------------------
// SCRIE UN INOD MODIFICAT ÎNAPOI ÎN TABELĂ
// -------------------------------------------------------------------------
int ext2_write_inode(uint32_t inode_num, ext2_inode_t* in_inode) {
    if (inode_num == 0) return 0;
    
    uint32_t block_size = 1024 << ext2_superblock.s_log_block_size;
    uint32_t inode_size = (ext2_superblock.s_rev_level >= 1) ? ext2_superblock.s_inode_size : 128;
    
    uint32_t group = (inode_num - 1) / ext2_superblock.s_inodes_per_group;
    uint32_t index = (inode_num - 1) % ext2_superblock.s_inodes_per_group;
    
    uint32_t bgdt_block = ext2_superblock.s_first_data_block + 1;
    uint32_t desc_size = sizeof(ext2_block_group_desc_t);
    uint32_t bg_block_offset = (group * desc_size) / block_size;
    uint32_t bg_offset_in_block = (group * desc_size) % block_size;
    
    uint8_t* temp_buf = (uint8_t*)malloc(block_size);
    if (!temp_buf) return 0;
    
    // 1. Citim Descriptorul de Grup ca să aflăm unde e Tabela de Inoduri
    ext2_read_block(bgdt_block + bg_block_offset, temp_buf);
    ext2_block_group_desc_t bgd;
    char* src = (char*)(temp_buf + bg_offset_in_block);
    for (uint32_t i = 0; i < desc_size; i++) ((char*)&bgd)[i] = src[i];
    
    uint32_t inode_table_block = bgd.bg_inode_table;
    uint32_t inode_block_offset = (index * inode_size) / block_size;
    uint32_t inode_offset_in_block = (index * inode_size) % block_size;
    
    // 2. Citim Blocul ce conține Inodul (ca să nu stricăm celelalte inoduri vecine)
    ext2_read_block(inode_table_block + inode_block_offset, temp_buf);
    
    // 3. Suprascriem strict octeții Inodului nostru
    char* dst = (char*)(temp_buf + inode_offset_in_block);
    char* s_in = (char*)in_inode;
    for (uint32_t i = 0; i < sizeof(ext2_inode_t); i++) dst[i] = s_in[i];
    
    // 4. Scriem Blocul înapoi pe disc
    ext2_write_block(inode_table_block + inode_block_offset, temp_buf);
    
    free(temp_buf);
    return 1;
}



// -------------------------------------------------------------------------
// GĂSEȘTE ȘI ALOCĂ PRIMUL BLOC DE DATE LIBER
// -------------------------------------------------------------------------
uint32_t ext2_alloc_block(void) {
    uint32_t block_size = 1024 << ext2_superblock.s_log_block_size;
    uint32_t total_groups = (ext2_superblock.s_blocks_count + ext2_superblock.s_blocks_per_group - 1) / ext2_superblock.s_blocks_per_group;
    uint32_t bgdt_block = ext2_superblock.s_first_data_block + 1;
    
    uint8_t* buf = (uint8_t*)malloc(block_size);
    uint8_t* bitmap_buf = (uint8_t*)malloc(block_size);

    for (uint32_t g = 0; g < total_groups; g++) {
        uint32_t bg_block_offset = (g * sizeof(ext2_block_group_desc_t)) / block_size;
        uint32_t bg_offset_in_block = (g * sizeof(ext2_block_group_desc_t)) % block_size;
        
        ext2_read_block(bgdt_block + bg_block_offset, buf);
        ext2_block_group_desc_t* bgd = (ext2_block_group_desc_t*)(buf + bg_offset_in_block);

        if (bgd->bg_free_blocks_count > 0) {
            ext2_read_block(bgd->bg_block_bitmap, bitmap_buf);
            
            // Căutăm bitul 0 (liber)
            for (uint32_t bit = 0; bit < ext2_superblock.s_blocks_per_group; bit++) {
                if (!(bitmap_buf[bit / 8] & (1 << (bit % 8)))) {
                    
                    // Îl marcăm ca OCUPAT (1) și salvăm Bitmap-ul
                    bitmap_buf[bit / 8] |= (1 << (bit % 8));
                    ext2_write_block(bgd->bg_block_bitmap, bitmap_buf);

                    // Scădem contorul și salvăm Descriptorul
                    bgd->bg_free_blocks_count--;
                    ext2_write_block(bgdt_block + bg_block_offset, buf);

                    ext2_superblock.s_free_blocks_count--; // (Opțional: scriem SB-ul pe disc)

                    free(buf); free(bitmap_buf);
                    return (g * ext2_superblock.s_blocks_per_group) + ext2_superblock.s_first_data_block + bit;
                }
            }
        }
    }
    free(buf); free(bitmap_buf);
    return 0; // Disk Full
}

// -------------------------------------------------------------------------
// GĂSEȘTE ȘI ALOCĂ PRIMUL INOD LIBER
// -------------------------------------------------------------------------
uint32_t ext2_alloc_inode(void) {
    uint32_t block_size = 1024 << ext2_superblock.s_log_block_size;
    uint32_t total_groups = (ext2_superblock.s_inodes_count + ext2_superblock.s_inodes_per_group - 1) / ext2_superblock.s_inodes_per_group;
    uint32_t bgdt_block = ext2_superblock.s_first_data_block + 1;
    
    uint8_t* buf = (uint8_t*)malloc(block_size);
    uint8_t* bitmap_buf = (uint8_t*)malloc(block_size);

    for (uint32_t g = 0; g < total_groups; g++) {
        uint32_t bg_block_offset = (g * sizeof(ext2_block_group_desc_t)) / block_size;
        uint32_t bg_offset_in_block = (g * sizeof(ext2_block_group_desc_t)) % block_size;
        
        ext2_read_block(bgdt_block + bg_block_offset, buf);
        ext2_block_group_desc_t* bgd = (ext2_block_group_desc_t*)(buf + bg_offset_in_block);

        if (bgd->bg_free_inodes_count > 0) {
            ext2_read_block(bgd->bg_inode_bitmap, bitmap_buf);
            
            for (uint32_t bit = 0; bit < ext2_superblock.s_inodes_per_group; bit++) {
                if (!(bitmap_buf[bit / 8] & (1 << (bit % 8)))) {
                    
                    bitmap_buf[bit / 8] |= (1 << (bit % 8));
                    ext2_write_block(bgd->bg_inode_bitmap, bitmap_buf);

                    bgd->bg_free_inodes_count--;
                    ext2_write_block(bgdt_block + bg_block_offset, buf);

                    ext2_superblock.s_free_inodes_count--;

                    free(buf); free(bitmap_buf);
                    // Inodurile încep de la 1, nu de la 0
                    return (g * ext2_superblock.s_inodes_per_group) + bit + 1;
                }
            }
        }
    }
    free(buf); free(bitmap_buf);
    return 0; // Fără Inoduri libere
}

// Curăță un string de cale: rezolvă "." și "..", și ignoră slash-urile multiple "//"
static void normalize_path(char* path) {
    char result[256];
    int res_len = 0;
    
    result[0] = '/';
    res_len = 1;

    int i = 0;
    while (path[i] != '\0') {
        while (path[i] == '/') i++; // Ignorăm slash-urile consecutive
        if (path[i] == '\0') break;

        // Identificăm lungimea segmentului (ex: "test2")
        int start = i;
        while (path[i] != '\0' && path[i] != '/') i++;
        int seg_len = i - start;

        if (seg_len == 1 && path[start] == '.') {
            // Este '.' -> Ignorăm, rămânem pe loc
        } else if (seg_len == 2 && path[start] == '.' && path[start + 1] == '.') {
            // Este '..' -> Ștergem ultimul folder din stringul `result`
            if (res_len > 1) {
                res_len--; // Scoatem ultimul caracter
                // Dăm înapoi până dăm de slash-ul precedent
                while (res_len > 1 && result[res_len - 1] != '/') {
                    res_len--;
                }
                if (res_len > 1) {
                    res_len--; // Scoatem și slash-ul ca să lăsăm calea curată
                }
            }
        } else {
            // Este un folder normal -> Îl adăugăm în string
            if (res_len > 1) {
                result[res_len++] = '/';
            }
            for (int j = 0; j < seg_len; j++) {
                result[res_len++] = path[start + j];
            }
        }
    }
    
    result[res_len] = '\0';
    
    // Suprascriem vechiul string cu cel curățat
    int k = 0;
    for (; k < res_len; k++) path[k] = result[k];
    path[k] = '\0';
}


// -------------------------------------------------------------------------
// ADAUGĂ UN NUME ÎN DIRECTORUL PĂRINTE
// -------------------------------------------------------------------------
int ext2_add_dir_entry(uint32_t dir_inode_num, const char* name, uint32_t inode_num) {
    ext2_inode_t dir_inode;
    if (!ext2_read_inode(dir_inode_num, &dir_inode)) return 0;

    uint32_t block_size = 1024 << ext2_superblock.s_log_block_size;
    uint8_t* buf = (uint8_t*)malloc(block_size);

    int name_len = 0;
    while (name[name_len]) name_len++;

    // Spațiul real NECESAR (8 octeți antet + numele, aliniat la multiplu de 4)
    uint32_t new_entry_len = 8 + name_len;
    if (new_entry_len % 4 != 0) new_entry_len += 4 - (new_entry_len % 4);

    // Căutăm prin blocurile directe ale directorului
    for (int i = 0; i < 12; i++) {
        uint32_t block_num = dir_inode.i_block[i];
        if (block_num == 0) continue; // Ar trebui extins directorul, dar pt simplitate sărim

        ext2_read_block(block_num, buf);
        uint32_t offset = 0;

        while (offset < block_size) {
            ext2_dir_entry_t* entry = (ext2_dir_entry_t*)(buf + offset);
            if (entry->rec_len == 0) break;

            // Cât spațiu ocupă de fapt această intrare veche?
            uint32_t real_len = 8 + entry->name_len;
            if (real_len % 4 != 0) real_len += 4 - (real_len % 4);

            // Dacă este ultima intrare din bloc, ea acoperă tot golul!
            // Verificăm dacă are loc să o micșorăm ca să facem loc celei noi
            if (entry->rec_len >= real_len + new_entry_len) {
                uint32_t old_rec_len = entry->rec_len;
                
                // 1. Tăiem intrarea veche la minimul necesar
                entry->rec_len = real_len;

                // 2. Creăm intrarea nouă fix în spațiul salvat!
                ext2_dir_entry_t* new_ent = (ext2_dir_entry_t*)(buf + offset + real_len);
                new_ent->inode = inode_num;
                new_ent->rec_len = old_rec_len - real_len; // Umple restul blocului
                new_ent->name_len = name_len;
                //new_ent->file_type = 2; // Punem 2 (Directory). (Dacă era fișier, puneam 1).
                // Verificăm ce tip de inod tocmai am adăugat pentru a seta iconița corectă
                ext2_inode_t child_ino;
                ext2_read_inode(inode_num, &child_ino);
                new_ent->file_type = ((child_ino.i_mode & 0xF000) == 0x4000) ? 2 : 1; // 2=DIR, 1=FILE
                
                for (int c = 0; c < name_len; c++) {
                    new_ent->name[c] = name[c];
                }
                
                // Salvăm blocul modificat
                ext2_write_block(block_num, buf);
                free(buf);
                return 1;
            }
            offset += entry->rec_len;
        }
    }
    
    free(buf);
    return 0; // Folder plin
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
    if (!total_sectors || !free_sectors) return 0;

    // 1. Calculăm dimensiunea unui bloc EXT2 în bytes (ex: 1024, 2048 sau 4096)
    uint32_t block_size = 1024 << ext2_superblock.s_log_block_size;
    
    // 2. Aflăm câte sectoare fizice de 512 bytes încap într-un bloc EXT2
    uint32_t sectors_per_block = block_size / 512;

    // 3. Extragem datele direct din Superblock (deja citit în memorie la ext2_init)
    *total_sectors = ext2_superblock.s_blocks_count * sectors_per_block;
    *free_sectors = ext2_superblock.s_free_blocks_count * sectors_per_block;

    return 1; 
}


static uint32_t ext2_find_in_dir(uint32_t dir_inode_num, const char* target_name) {
    if (target_name[0] == '\0') return dir_inode_num;

    ext2_inode_t dir_inode;
    if (!ext2_read_inode(dir_inode_num, &dir_inode)) return 0;

    // Ne asigurăm că inodul părinte chiar e director
    if ((dir_inode.i_mode & 0xF000) != 0x4000) return 0;

    uint32_t block_size = 1024 << ext2_superblock.s_log_block_size;
    uint8_t* block_buffer = (uint8_t*)malloc(block_size);
    if (!block_buffer) return 0;

    uint32_t bytes_read = 0;
    
    // Căutăm prin blocurile directe
    for (int i = 0; i < 12 && bytes_read < dir_inode.i_size; i++) {
        uint32_t block_num = dir_inode.i_block[i];
        if (block_num == 0) break;

        ext2_read_block(block_num, block_buffer);
        uint32_t offset = 0;

        while (offset < block_size && bytes_read < dir_inode.i_size) {
            ext2_dir_entry_t* entry = (ext2_dir_entry_t*)(block_buffer + offset);
            if (entry->rec_len == 0) break; // Prevenim bucle infinite la corupere

            if (entry->inode != 0) {
                char name[256];
                int name_l = entry->name_len;
                if (name_l > 255) name_l = 255;
                for (int n = 0; n < name_l; n++) name[n] = entry->name[n];
                name[name_l] = '\0';

                // Am găsit elementul!
                if (streq(name, target_name)) {
                    uint32_t found_inode = entry->inode;
                    free(block_buffer);
                    return found_inode;
                }
            }
            offset += entry->rec_len;
            bytes_read += entry->rec_len;
        }
    }
    
    free(block_buffer);
    return 0; // Nu a fost găsit
}


// Sparge o cale complexă (ex: "/test/test2/test3") și returnează inodul destinație
static uint32_t ext2_resolve_path(const char* path) {
    if (!path || path[0] == '\0') return ext2_current_inode;
    if (streq(path, "/")) return 2; // Inodul Root

    // Dacă începe cu '/', căutăm din Root. Altfel, căutăm din folderul curent.
    uint32_t current_ino = (path[0] == '/') ? 2 : ext2_current_inode;
    const char* p = (path[0] == '/') ? path + 1 : path;

    char segment[256];
    while (*p) {
        int i = 0;
        // Extragem următorul segment până la '/' sau la finalul șirului
        while (*p && *p != '/' && i < 63) {
            segment[i++] = *p++;
        }
        segment[i] = '\0';

        // Sărim peste '/' multiple, în caz că utilizatorul tastează "folder//subfolder"
        while (*p == '/') p++;

        if (segment[0] == '\0') continue;

        // Căutăm segmentul în inodul la care am ajuns
        uint32_t next_ino = ext2_find_in_dir(current_ino, segment);
        if (next_ino == 0) return 0; // O parte din cale nu există!

        current_ino = next_ino;
    }

    return current_ino;
}

static void ext2_list_files(const char* path) {
    // 1. În EXT2, Inodul Rădăcină (Root) este mereu 2.
    uint32_t target_inode_num = ext2_current_inode; 

    // Dacă utilizatorul dă "ls folder" (o cale anume)
    if (path && path[0] != '\0' && !streq(path, ".") && !streq(path, "/")) {
        target_inode_num = ext2_resolve_path(path);
        if (target_inode_num == 0) {
            print("[EXT2] Fisier sau director inexistent.\n");
            return;
        }
    } else if (path && streq(path, "/")) {
        target_inode_num = 2; // Explicit cere root-ul
    }

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
    uint32_t inode_num = ext2_resolve_path(name);

    if (inode_num == 0) {
        //print("[EXT2] Fisierul nu a fost gasit.\n");
        return 0;
    }

    // 2. Citim structura inodului
    ext2_inode_t file_inode;
    if (!ext2_read_inode(inode_num, &file_inode)) return 0;

    // Verificăm dacă e fișier normal (0x8000 = Regular File)
    if ((file_inode.i_mode & 0xF000) != 0x8000) {
        //print("[EXT2] Calea nu este un fisier valid!\n");
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

// Caută un nume de fișier/director în interiorul unui alt director dat prin 'dir_inode_num'






static int ext2_cd(const char* name) {
    if (!name || name[0] == '\0') return 0;

    // 1. Folosim noul resolver pentru a obține inodul final al căii
    uint32_t target_inode = ext2_resolve_path(name);
    if (target_inode == 0) return 0; // Calea invalidă

    // 2. Citim inodul pentru a ne asigura că este într-adevăr director, nu fișier
    ext2_inode_t inode;
    if (!ext2_read_inode(target_inode, &inode)) return 0;

    if ((inode.i_mode & 0xF000) != 0x4000) {
        print("[EXT2] Tinta nu este un director!\n");
        return 0; 
    }

    // 3. Actualizăm starea internă a driverului
    ext2_current_inode = target_inode;

    // 4. Actualizăm prompt-ul vizual pentru shell (ext2_cwd_path)
    if (name[0] == '/') {
        int i = 0;
        while (name[i] && i < 255) { ext2_cwd_path[i] = name[i]; i++; }
        ext2_cwd_path[i] = '\0';
    } else {
        int len = 0; while (ext2_cwd_path[len]) len++;
        if (len > 0 && ext2_cwd_path[len - 1] != '/') ext2_cwd_path[len++] = '/';
        int j = 0;
        while (name[j] && len < 255) ext2_cwd_path[len++] = name[j++];
        ext2_cwd_path[len] = '\0';
    }

    // --- NOU: Curățăm calea rezultată ---
    normalize_path(ext2_cwd_path);

    return 1;
}


static void ext2_get_current_path(char* buffer, uint32_t max_len) {
    int i = 0;
    while (ext2_cwd_path[i] && i < (int)max_len - 1) {
        buffer[i] = ext2_cwd_path[i];
        i++;
    }
    buffer[i] = '\0';
}

// -------------------------------------------------------------------------
// CREARE FIȘIER GOL (0 bytes)
// -------------------------------------------------------------------------
// Separă calea părintelui de numele real al fișierului (ex: "./aa" -> Părinte: curent, Nume: "aa")
static uint32_t ext2_get_parent_and_name(const char* full_path, char* out_filename) {
    int len = 0;
    int last_slash = -1;
    while (full_path[len] && len < 255) {
        if (full_path[len] == '/') last_slash = len;
        len++;
    }
    
    // Cazul 1: Nu avem niciun slash (ex: "aa") -> Suntem în directorul curent
    if (last_slash == -1) {
        for (int i = 0; i <= len; i++) out_filename[i] = full_path[i];
        return ext2_current_inode;
    }
    
    // Cazul 2: Avem slash. Copiem doar Numele (ce e după ultimul slash)
    int j = 0;
    for (int i = last_slash + 1; i <= len; i++) {
        out_filename[j++] = full_path[i];
    }
    
    // Extragem Calea Părintelui
    char parent_path[256];
    if (last_slash == 0) {
        return 2; // Fișierul e fix în Root (ex: "/aa")
    } else {
        for (int i = 0; i < last_slash; i++) parent_path[i] = full_path[i];
        parent_path[last_slash] = '\0';
        
        if (streq(parent_path, ".")) return ext2_current_inode; // Gestionăm "./"
        
        return ext2_resolve_path(parent_path); // Găsim inodul folderului folosind funcția noastră
    }
}

static int ext2_create_file(const char* name, uint32_t size) {
    char pure_name[256];
    uint32_t parent_ino = ext2_get_parent_and_name(name, pure_name);
    if (parent_ino == 0 || pure_name[0] == '\0') return 0;
    
    // Căutăm în inodul PĂRINTELUI, folosind numele PUR
    if (ext2_find_in_dir(parent_ino, pure_name) != 0) {
        return 0; 
    }

    uint32_t new_inode = ext2_alloc_inode();
    if (new_inode == 0) return 0;

    ext2_inode_t inode;
    uint8_t* ptr = (uint8_t*)&inode;
    for (uint32_t i = 0; i < sizeof(ext2_inode_t); i++) ptr[i] = 0;

    inode.i_mode = 0x8000 | 0644; 
    inode.i_size = 0;
    inode.i_blocks = 0;

    ext2_write_inode(new_inode, &inode);

    // Scriem în folderul părinte corect
    if (!ext2_add_dir_entry(parent_ino, pure_name, new_inode)) return 0;
    return 1;
}

static int ext2_write_file(const char* name, const uint8_t* data, uint32_t size) {
    if (!name || !data) return 0;

    char pure_name[256];
    uint32_t parent_ino = ext2_get_parent_and_name(name, pure_name);
    if (parent_ino == 0 || pure_name[0] == '\0') return 0;

    uint32_t target_inode = ext2_find_in_dir(parent_ino, pure_name);
    if (target_inode == 0) {
        if (!ext2_create_file(name, 0)) return 0;
        target_inode = ext2_find_in_dir(parent_ino, pure_name);
        if (target_inode == 0) return 0; 
    }

    ext2_inode_t inode;
    if (!ext2_read_inode(target_inode, &inode)) return 0;

    if ((inode.i_mode & 0xF000) != 0x8000) {
        print("[EXT2] Eroare: Tinta este un director!\n");
        return 0;
    }
    
    uint32_t block_size = 1024 << ext2_superblock.s_log_block_size;
    uint32_t blocks_needed = (size + block_size - 1) / block_size;

    if (blocks_needed > 12) return 0;

    uint32_t bytes_written = 0;
    uint8_t* temp_buf = (uint8_t*)malloc(block_size);
    if (!temp_buf) return 0;

    for (uint32_t i = 0; i < blocks_needed; i++) {
        if (inode.i_block[i] == 0) {
            inode.i_block[i] = ext2_alloc_block();
            if (inode.i_block[i] == 0) break;
            inode.i_blocks += (block_size / 512);
        }
        uint32_t chunk = (size - bytes_written > block_size) ? block_size : (size - bytes_written);
        for (uint32_t j = 0; j < chunk; j++) temp_buf[j] = data[bytes_written + j];
        for (uint32_t j = chunk; j < block_size; j++) temp_buf[j] = 0;
        ext2_write_block(inode.i_block[i], temp_buf);
        bytes_written += chunk;
    }
    inode.i_size = bytes_written;
    ext2_write_inode(target_inode, &inode);
    free(temp_buf);
    return bytes_written;
}

static int ext2_delete_file(const char* name) {
    char pure_name[256];
    uint32_t parent_ino = ext2_get_parent_and_name(name, pure_name);
    if (parent_ino == 0 || pure_name[0] == '\0') return 0;

    ext2_inode_t dir_inode;
    // Citim PĂRINTELE pentru a căuta fișierul
    if (!ext2_read_inode(parent_ino, &dir_inode)) return 0;

    uint32_t block_size = 1024 << ext2_superblock.s_log_block_size;
    uint8_t* buf = (uint8_t*)malloc(block_size);
    if (!buf) return 0;

    for (int i = 0; i < 12; i++) {
        uint32_t block_num = dir_inode.i_block[i];
        if (block_num == 0) continue;
        ext2_read_block(block_num, buf);
        uint32_t offset = 0;

        while (offset < block_size) {
            ext2_dir_entry_t* entry = (ext2_dir_entry_t*)(buf + offset);
            if (entry->rec_len == 0) break;

            if (entry->inode != 0) {
                char fname[256];
                int name_l = entry->name_len;
                if (name_l > 255) name_l = 255;
                for (int n = 0; n < name_l; n++) fname[n] = entry->name[n];
                fname[name_l] = '\0';

                // Verificăm pe baza numelui PUR
                if (streq(fname, pure_name)) {
                    entry->inode = 0; 
                    ext2_write_block(block_num, buf);
                    free(buf);
                    return 1;
                }
            }
            offset += entry->rec_len;
        }
    }
    free(buf);
    return 0; 
}

static int ext2_mkdir(const char* name) {
    if (!name || name[0] == '\0') 
        return 0;

    // 1. Verificăm dacă există deja în inodul curent
    if (ext2_find_in_dir(ext2_current_inode, name) != 0)
        return 0; 

    // 2. Alocăm inode și block (Necesită implementarea funcțiilor de manipulare a bitmap-urilor)
    uint32_t new_inode = ext2_alloc_inode();
    if (new_inode == 0) return 0;

    uint32_t new_block = ext2_alloc_block();
    if (new_block == 0) return 0;

    // 3. Inițializăm inode-ul nou ca director (0x4000 este masca pentru Director)
    ext2_inode_t inode;
    uint8_t* ptr = (uint8_t*)&inode;
    for (uint32_t i = 0; i < sizeof(ext2_inode_t); i++) ptr[i] = 0;

    uint32_t block_size = 1024 << ext2_superblock.s_log_block_size;

    inode.i_mode = 0x4000 | 0755; 
    inode.i_size = block_size; 
    inode.i_blocks = block_size / 512; // EXT2 contorizează mereu în sectoare de 512 bytes
    inode.i_block[0] = new_block;

    ext2_write_inode(new_inode, &inode);

    // 4. Scriem "." și ".." în block-ul nou
    uint8_t* buffer = (uint8_t*)malloc(block_size);
    if (!buffer) return 0;
    for (uint32_t i = 0; i < block_size; i++) buffer[i] = 0;

    // Construim înregistrarea "."
    ext2_dir_entry_t* dot = (ext2_dir_entry_t*)buffer;
    dot->inode = new_inode;
    dot->name_len = 1;
    dot->file_type = 2; // EXT2_FT_DIR
    dot->rec_len = 12;  // Dimensiunea minimă aliniată la 4 bytes
    dot->name[0] = '.';

    // Construim înregistrarea ".." care ocupă restul blocului liber
    ext2_dir_entry_t* dotdot = (ext2_dir_entry_t*)(buffer + dot->rec_len);
    dotdot->inode = ext2_current_inode;
    dotdot->name_len = 2;
    dotdot->file_type = 2; // EXT2_FT_DIR
    dotdot->rec_len = block_size - dot->rec_len; // Restul blocului
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';

    ext2_write_block(new_block, buffer);
    free(buffer);

    // 5. Adăugăm entry în directorul părinte
    if (!ext2_add_dir_entry(ext2_current_inode, name, new_inode))
        return 0;

    return 1;
}




static void ext2_fdisk(void) {
    print("[EXT2] Operatiunea 'fdisk' trebuie rulata la nivel de disc (ATA), nu pe sistemul de fisiere.\n");
}

static void ext2_format(void) {
    print("[EXT2] Formatarea (mkfs.ext2) nu este suportata intern momentan.\n");
    print("[EXT2] Utilizeaza utilitarele host-ului (Linux) pentru a formata discul inainte de utilizare.\n");
}


// -------------------------------------------------------------------------
// ȘTERGERE DIRECTOR (Soft Delete)
// -------------------------------------------------------------------------
static int ext2_rmdir(const char* name) {
    char pure_name[256];
    uint32_t parent_ino = ext2_get_parent_and_name(name, pure_name);
    if (parent_ino == 0 || pure_name[0] == '\0') return 0;

    // Găsim directorul țintă
    uint32_t target_inode = ext2_find_in_dir(parent_ino, pure_name);
    if (target_inode == 0) return 0;

    // Verificăm dacă este cu adevărat director (0x4000)
    ext2_inode_t inode;
    if (!ext2_read_inode(target_inode, &inode)) return 0;
    if ((inode.i_mode & 0xF000) != 0x4000) return 0; // Nu este director

    // Citim folderul părinte ca să ștergem intrarea
    ext2_inode_t dir_inode;
    if (!ext2_read_inode(parent_ino, &dir_inode)) return 0;

    uint32_t block_size = 1024 << ext2_superblock.s_log_block_size;
    uint8_t* buf = (uint8_t*)malloc(block_size);
    if (!buf) return 0;

    // Căutăm și anulăm intrarea (soft delete)
    for (int i = 0; i < 12; i++) {
        uint32_t block_num = dir_inode.i_block[i];
        if (block_num == 0) continue;
        ext2_read_block(block_num, buf);
        uint32_t offset = 0;

        while (offset < block_size) {
            ext2_dir_entry_t* entry = (ext2_dir_entry_t*)(buf + offset);
            if (entry->rec_len == 0) break;

            if (entry->inode != 0) {
                char fname[256];
                int name_l = entry->name_len;
                if (name_l > 255) name_l = 255;
                for (int n = 0; n < name_l; n++) fname[n] = entry->name[n];
                fname[name_l] = '\0';

                if (streq(fname, pure_name)) {
                    // Setăm inodul pe 0 ca să dispară logic
                    entry->inode = 0;
                    ext2_write_block(block_num, buf);
                    free(buf);
                    return 1;
                }
            }
            offset += entry->rec_len;
        }
    }
    free(buf);
    return 0;
}

// -------------------------------------------------------------------------
// PREIA NUMELE FIȘIERULUI LA UN ANUMIT INDEX (Pentru Shell / Autocomplete)
// -------------------------------------------------------------------------
static int ext2_get_file_at_index(int index, char* buffer, uint32_t max_len) {
    if (!buffer || max_len == 0) return 0;

    ext2_inode_t dir_inode;
    // Citim mereu din directorul curent
    if (!ext2_read_inode(ext2_current_inode, &dir_inode)) return 0;

    uint32_t block_size = 1024 << ext2_superblock.s_log_block_size;
    uint8_t* block_buffer = (uint8_t*)malloc(block_size);
    if (!block_buffer) return 0;

    uint32_t bytes_read = 0;
    int current_idx = 0;

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

                // Ignorăm "." și ".." pentru a se potrivi cu listarea de la 'ls'
                if (!streq(name, ".") && !streq(name, "..")) {
                    if (current_idx == index) {
                        // Am găsit indexul! Copiem numele
                        int copy_len = 0;
                        while (name[copy_len] && copy_len < (int)max_len - 1) {
                            buffer[copy_len] = name[copy_len];
                            copy_len++;
                        }
                        buffer[copy_len] = '\0';
                        free(block_buffer);
                        return 1;
                    }
                    current_idx++;
                }
            }
            offset += entry->rec_len;
            bytes_read += entry->rec_len;
        }
    }
    
    free(block_buffer);
    return 0; // Nu a fost găsit indexul
}

// -------------------------------------------------------------------------
// APPEND FIȘIER (Adaugă date la sfârșitul unui fișier)
// -------------------------------------------------------------------------
static int ext2_append_file(const char* path, const char* data) {
    if (!path || !data) return 0;

    // Calculăm lungimea string-ului de date ce trebuie adăugat
    uint32_t data_len = 0;
    while (data[data_len]) data_len++;
    if (data_len == 0) return 1; // Nu avem ce adăuga

    char pure_name[256];
    uint32_t parent_ino = ext2_get_parent_and_name(path, pure_name);
    if (parent_ino == 0 || pure_name[0] == '\0') return 0;

    uint32_t target_inode = ext2_find_in_dir(parent_ino, pure_name);
    uint32_t existing_size = 0;

    if (target_inode != 0) {
        // Dacă fișierul există deja, aflăm cât de mare este
        ext2_inode_t inode;
        if (ext2_read_inode(target_inode, &inode)) {
            if ((inode.i_mode & 0xF000) != 0x8000) return 0; // Eroare: e director
            existing_size = inode.i_size;
        }
    }

    uint32_t new_total_size = existing_size + data_len;
    
    // Verificare de siguranță: nu permitem fișiere mai mari de 12 blocuri
    uint32_t block_size = 1024 << ext2_superblock.s_log_block_size;
    if (new_total_size > 12 * block_size) return 0;

    uint8_t* temp_buffer = (uint8_t*)malloc(new_total_size);
    if (!temp_buffer) return 0;

    // 1. Încărcăm fișierul existent în memorie
    if (existing_size > 0) {
        ext2_read_file(path, temp_buffer, existing_size);
    }

    // 2. Adăugăm datele noi la finalul buffer-ului
    for (uint32_t i = 0; i < data_len; i++) {
        temp_buffer[existing_size + i] = data[i];
    }

    // 3. Suprascriem totul folosind funcția noastră robustă de write
    int bytes_written = ext2_write_file(path, temp_buffer, new_total_size);
    
    free(temp_buffer);
    return (bytes_written == (int)new_total_size) ? 1 : 0;
}

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