#include <stdint.h>
#include "fs.h"
#include "fs_v3.h"
#include "fs_v3_inode.h"
#include "fs_v3_dir.h"
#include "../syslog.h"

// Variabilă globală în driver care ține minte calea exactă
static char current_v3_path[256] = "/";

// Funcții externe de disk I/O pe hdb (drive 1)
extern void disk_read_sector_drive(uint8_t drive, uint32_t lba, uint8_t* buffer);
extern void disk_write_sector_drive(uint8_t drive, uint32_t lba, const uint8_t* buffer);

extern uint8_t current_drive_id;

// Funcții externe FAT
extern uint32_t nfs3_alloc_sector(void);
extern uint32_t nfs3_append_sector(uint32_t last_sector);
extern uint32_t nfs3_read_fat_entry(uint32_t disk_sector);
extern void nfs3_write_fat_entry(uint32_t disk_sector, uint32_t value);
extern void nfs3_free_chain(uint32_t start_sector);

// Funcții de afișare din kernel
extern void print(const char* s);
extern void print_number(uint32_t n);
extern void newline();

// Starea curentă a navigației în NanoFS V3 (Indexul inodului directorului curent)
uint32_t current_dir_inode = 0; // Root este inodul 0

// Funcție internă de comparație
static int nfs3_streq(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

// Caută și rezolvă o cale completă, returnând inodul părintelui și numele fișierului la capăt
// Caută și rezolvă o cale completă, returnând inodul părintelui.
// Returnează 0xFFFFFFFF dacă a apărut o eroare (calea nu există).
static uint32_t nfs3_resolve_path(const char* full_path, char* filename_out) {
    if (!full_path || !full_path[0]) return 0xFFFFFFFF; // Eroare

    char path_copy[128];
    int p = 0;
    while (full_path[p] && p < 127) {
        path_copy[p] = full_path[p];
        p++;
    }
    path_copy[p] = '\0';

    int last_slash_idx = -1;
    for (int i = 0; path_copy[i] != '\0'; i++) {
        if (path_copy[i] == '/') last_slash_idx = i;
    }

    if (last_slash_idx == -1) {
        int i = 0;
        while (path_copy[i] && i < 63) {
            filename_out[i] = path_copy[i];
            i++;
        }
        filename_out[i] = '\0';
        return current_dir_inode; // Părintele este directorul curent (poate fi 0)
    }

    int i = 0;
    int fn_start = last_slash_idx + 1;
    while (path_copy[fn_start + i] && i < 63) {
        filename_out[i] = path_copy[fn_start + i];
        i++;
    }
    filename_out[i] = '\0';

    char dir_path[128];
    if (last_slash_idx == 0) {
        dir_path[0] = '/';
        dir_path[1] = '\0';
    } else {
        for (i = 0; i < last_slash_idx; i++) {
            dir_path[i] = path_copy[i];
        }
        dir_path[i] = '\0';
    }

    uint32_t temp_inode = (dir_path[0] == '/') ? 0 : current_dir_inode;
    char* token_path = dir_path;
    if (token_path[0] == '/') token_path++;

    while (*token_path) {
        while (*token_path == '/') token_path++;
        if (*token_path == '\0') break;

        char folder[64];
        int f = 0;
        while (*token_path != '/' && *token_path != '\0' && f < 63) {
            folder[f++] = *token_path++;
        }
        folder[f] = '\0';
        while (*token_path != '/' && *token_path != '\0') token_path++;

        if (nfs3_streq(folder, ".")) continue;
        if (nfs3_streq(folder, "..")) continue; 

        uint32_t child_inode = nfs3_find_in_dir(temp_inode, folder);
        if (child_inode == 0) return 0xFFFFFFFF; // Eroare: Directorul nu există!

        InodeV3 child_inv;
        nfs3_read_inode(child_inode, &child_inv);
        if (child_inv.flags != INODE_FLAG_DIR) return 0xFFFFFFFF; // Eroare: Nu e director!

        temp_inode = child_inode;
    }

    return temp_inode;
}

// --- Implementarea interfeței VFS pentru V3 ---

void fs_init_v3(void) {
    uint8_t buffer[512];
    disk_read_sector_drive(current_drive_id, 1, buffer);
    SuperblockV3* sb = (SuperblockV3*)buffer;

    if (sb->magic[0] == NFS3_MAGIC[0] && sb->magic[1] == NFS3_MAGIC[1] &&
        sb->magic[2] == NFS3_MAGIC[2] && sb->magic[3] == NFS3_MAGIC[3]) {
        KLOG_INFO("[NFS3] Driver montat cu succes pe hdb.");
    } else {
        KLOG_WARNING("[NFS3] Disc hdb neformatat. Se rulează formatarea V3...");
        nfs3_format();
    }
    current_dir_inode = 0; // Root
}

void fs_list_files_v3(const char* path) {
    uint32_t target_inode = current_dir_inode;

    if (path != 0 && path[0] != '\0' && !(path[0] == '.' && path[1] == '\0')) {
        char dummy[64];
        target_inode = nfs3_resolve_path(path, dummy);
        if (target_inode == 0 && !nfs3_streq(path, "/")) {
            print("Error: Directory does not exist.\n");
            return;
        }
        uint32_t found = nfs3_find_in_dir(target_inode, dummy);
        if (found != 0) {
            InodeV3 inv;
            nfs3_read_inode(found, &inv);
            if (inv.flags == INODE_FLAG_DIR) target_inode = found;
        }
    }

    nfs3_list_dir(target_inode);
}

int fs_create_file_v3(const char* name, uint32_t size) {
    char target_filename[64];
    if (!name || name[0] == '\0') return 0;

    uint32_t parent_inode = nfs3_resolve_path(name, target_filename);
    
    if (parent_inode == 0xFFFFFFFF) return 0; 
    if (target_filename[0] == '\0') return 0;

    return nfs3_create_entry(parent_inode, target_filename, INODE_FLAG_FILE, size);
}

int fs_delete_file_v3(const char* name) {
    char target_filename[64];
    if (!name || name[0] == '\0') return 0;

    uint32_t parent_inode = nfs3_resolve_path(name, target_filename);
    
    // VERIFICAREA CORECTĂ:
    if (parent_inode == 0xFFFFFFFF) return 0; 
    if (target_filename[0] == '\0') return 0;

    return nfs3_delete_entry(parent_inode, target_filename);
}

int fs_write_file_v3(const char* name, const uint8_t* data, uint32_t size) {
    char target_filename[64];
    if (!name || name[0] == '\0') return 0;

    uint32_t parent_inode = nfs3_resolve_path(name, target_filename);
    if (parent_inode == 0xFFFFFFFF || target_filename[0] == '\0') return 0;

    // Căutăm fișierul. Dacă nu există, încercăm să-l creăm automat!
    uint32_t target_inode_idx = nfs3_find_in_dir(parent_inode, target_filename);
    if (target_inode_idx == 0xFFFFFFFF) {
        if (!nfs3_create_entry(parent_inode, target_filename, INODE_FLAG_FILE, 0)) {
            return 0; // Eroare la creare
        }
        target_inode_idx = nfs3_find_in_dir(parent_inode, target_filename);
        if (target_inode_idx == 0xFFFFFFFF) return 0;
    }

    InodeV3 target_inode;
    if (!nfs3_read_inode(target_inode_idx, &target_inode)) return 0;

    if (target_inode.flags == INODE_FLAG_DIR) {
        KLOG_ERROR("[NFS3 WRITE] Nu poți scrie text într-un director!\n");
        return 0; 
    }

    // PASUL CRITIC: Eliberăm vechile sectoare (dacă avea) ca să scriem de la zero
    if (target_inode.first_sector != 0 && target_inode.first_sector != 0xFFFFFFFF) {
        nfs3_free_chain(target_inode.first_sector);
    }

    // Actualizăm mărimea inodului
    target_inode.size = size;

    if (size == 0) {
        target_inode.first_sector = 0;
        nfs3_write_inode(target_inode_idx, &target_inode);
        return 1;
    }

    // Scriem datele bloc cu bloc (512 bytes)
    uint32_t current_sector = nfs3_alloc_sector();
    if (current_sector == 0) return 0; // Disk full
    
    target_inode.first_sector = current_sector;
    uint32_t bytes_written = 0;

    while (bytes_written < size) {
        uint32_t to_write = size - bytes_written;
        if (to_write > 512) to_write = 512;

        uint8_t buffer[512] = {0}; // Curățăm buffer-ul
        for (uint32_t i = 0; i < to_write; i++) {
            buffer[i] = data[bytes_written + i];
        }

        disk_write_sector_drive(current_drive_id, current_sector, buffer);
        bytes_written += to_write;

        // Dacă mai avem date de scris, mai alocăm un sector în FAT
        if (bytes_written < size) {
            uint32_t next_sector = nfs3_append_sector(current_sector);
            if (next_sector == 0) break; // Disk Full
            current_sector = next_sector;
        }
    }

    // Salvăm inodul actualizat pe disc
    nfs3_write_inode(target_inode_idx, &target_inode);
    return 1;
}

int fs_read_file_v3(const char* name, uint8_t* buffer, uint32_t max_size) {
    char target_filename[64];
    if (!name || name[0] == '\0') return 0;

    uint32_t parent_inode = nfs3_resolve_path(name, target_filename);
    if (parent_inode == 0xFFFFFFFF || target_filename[0] == '\0') return 0;

    uint32_t target_inode_idx = nfs3_find_in_dir(parent_inode, target_filename);
    if (target_inode_idx == 0xFFFFFFFF) return 0; // Fișierul nu există

    InodeV3 target_inode;
    if (!nfs3_read_inode(target_inode_idx, &target_inode)) return 0;

    if (target_inode.flags == INODE_FLAG_DIR) return 0; // E director
    if (target_inode.size == 0 || target_inode.first_sector == 0) return 0; // E gol

    uint32_t current_sector = target_inode.first_sector;
    uint32_t bytes_read = 0;

    while (current_sector != 0xFFFFFFFF && current_sector != 0 && bytes_read < target_inode.size) {
        if (bytes_read >= max_size) break; // Am umplut bufferul cerut

        uint8_t sec_buf[512];
        disk_read_sector_drive(current_drive_id, current_sector, sec_buf);

        uint32_t to_read = target_inode.size - bytes_read;
        if (to_read > 512) to_read = 512;
        
        // Asigură-te că nu depășim max_size-ul programului din user-space
        if (bytes_read + to_read > max_size) to_read = max_size - bytes_read;

        for (uint32_t i = 0; i < to_read; i++) {
            buffer[bytes_read + i] = sec_buf[i];
        }

        bytes_read += to_read;
        current_sector = nfs3_read_fat_entry(current_sector); // Sărim la următorul sector din FAT
    }

    return bytes_read; // Returnăm câți bytes am reușit să citim
}

int fs_mkdir_v3(const char* name) {
    char target_filename[64];
    if (!name || name[0] == '\0') return 0;

    uint32_t parent_inode = nfs3_resolve_path(name, target_filename);
    
    // Verificăm dacă path-ul a fost greșit
    if (parent_inode == 0xFFFFFFFF) return 0; 
    
    // Verificăm dacă avem un nume valid
    if (target_filename[0] == '\0') return 0;

    return nfs3_create_entry(parent_inode, target_filename, INODE_FLAG_DIR, 0);
}

int fs_rmdir_v3(const char* name) {
    return fs_delete_file_v3(name);
}

int fs_cd_v3(const char* name) {
    if (nfs3_streq(name, "/")) {
        current_dir_inode = 0;
        current_v3_path[0] = '/';
        current_v3_path[1] = '\0';
        return 1;
    }

    char target_filename[64];
    uint32_t parent_inode = nfs3_resolve_path(name, target_filename);
    if (parent_inode == 0xFFFFFFFF) return 0;

    uint32_t target_inode = nfs3_find_in_dir(parent_inode, target_filename);
    if (target_inode != 0xFFFFFFFF) {
        InodeV3 inv;
        nfs3_read_inode(target_inode, &inv);
        if (inv.flags == INODE_FLAG_DIR) {
            current_dir_inode = target_inode;
            
            // --- ACTUALIZAREA CĂII PENTRU SHELL ---
            if (nfs3_streq(target_filename, "..")) {
                int len = 0;
                while (current_v3_path[len]) len++;
                if (len > 1) { // Nu putem merge mai sus de root '/'
                    for (int i = len - 1; i >= 0; i--) {
                        if (current_v3_path[i] == '/') {
                            if (i == 0) current_v3_path[1] = '\0'; // am ajuns la /
                            else current_v3_path[i] = '\0';
                            break;
                        }
                    }
                }
            } else if (!nfs3_streq(target_filename, ".")) {
                // Adăugăm noul folder la cale
                int len = 0;
                while (current_v3_path[len]) len++;
                if (current_v3_path[len - 1] != '/') current_v3_path[len++] = '/';
                int j = 0;
                while (target_filename[j]) current_v3_path[len++] = target_filename[j++];
                current_v3_path[len] = '\0';
            }
            return 1;
        }
    }
    return 0;
}

void fs_get_current_path_v3(char* buffer, uint32_t max_len) {
    uint32_t i = 0;
    while (current_v3_path[i] && i < max_len - 1) {
        buffer[i] = current_v3_path[i];
        i++;
    }
    buffer[i] = '\0';
}

void fs_fdisk_v3(void) {
    uint8_t buffer[512];
    disk_read_sector_drive(current_drive_id, 1, buffer);
    SuperblockV3* sb = (SuperblockV3*)buffer;

    print("--- NanoFS V3 Disk Analysis (hdb) ---\n");
    if (sb->magic[0] == 'N' && sb->magic[1] == 'F' && sb->magic[2] == 'S' && sb->magic[3] == '3') {
        print("File System : NanoFS V3 (Active on hdb)\n");
        print("Total Sectors: "); print_number(sb->total_sectors); newline();
        print("Inode Count  : "); print_number(sb->inode_count); newline();
    } else {
        print("Unformatted V3 Disk\n");
    }
    print("-------------------------------------\n");
}

void fs_format_v3(void) {
    nfs3_format();
    current_dir_inode = 0;
}

int nfs3_get_stats(uint32_t* total_sectors, uint32_t* free_sectors) {
    SuperblockV3 sb;
    read_superblock(&sb);
    
    *total_sectors = sb.total_sectors;
    uint32_t free_count = 0;

    // Citim FAT-ul eficient, sector cu sector, fără apeluri redundante care blochează discul
    uint32_t fat_sectors_count = (sb.total_sectors * sizeof(uint32_t) + 511) / 512;
    
    for (uint32_t s = 0; s < fat_sectors_count; s++) {
        uint8_t fat_buf[512];
        disk_read_sector_drive(current_drive_id, sb.fat_start_sector + s, fat_buf);
        uint32_t* entries = (uint32_t*)fat_buf;
        
        uint32_t entries_per_sector = 512 / sizeof(uint32_t);
        for (uint32_t i = 0; i < entries_per_sector; i++) {
            uint32_t global_idx = s * entries_per_sector + i;
            if (global_idx < sb.total_sectors) {
                if (entries[i] == 0) { // 0 = sector liber în FAT
                    free_count++;
                }
            }
        }
    }

    *free_sectors = free_count;
    return 1;
}

// Exportul oficial al driverului V3 compatibil VFS
FileSystemInterface nfs3_driver = {
    .name = "NFS3",
	.get_stats = nfs3_get_stats,
    .init = fs_init_v3,
    .list_files = fs_list_files_v3,
    .create_file = fs_create_file_v3,
    .delete_file = fs_delete_file_v3,
    .write_file = fs_write_file_v3,
    .read_file = fs_read_file_v3,
    .mkdir = fs_mkdir_v3,
    .rmdir = fs_rmdir_v3,
    .cd = fs_cd_v3,
    .get_current_path = fs_get_current_path_v3,
    .fdisk = fs_fdisk_v3,
    .format = fs_format_v3,
	.get_file_at_index = nfs3_get_file_at_index
};