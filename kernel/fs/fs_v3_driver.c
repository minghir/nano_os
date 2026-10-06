#include <stdint.h>
#include "fs.h"
#include "fs_v3.h"
#include "fs_v3_inode.h"
#include "fs_v3_dir.h"
#include "../syslog.h"
#include "../process.h" // Acces la structura PCB și current_process

extern PCB* current_process;

// Variabilă globală în driver care ține minte calea exactă
//static char current_v3_path[256] = "/";
// Starea curentă a navigației în NanoFS V3 (Indexul inodului directorului curent)
//uint32_t current_dir_inode = 0; // Root este inodul 0

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


uint32_t get_current_dir_inode(void) {
    if (current_process) {
        return current_process->cwd_sector;
    }
    return 0; // Fallback la Root
}

void set_current_dir_inode(uint32_t inode) {
    if (current_process) {
        current_process->cwd_sector = inode;
    }
}

void get_current_v3_path(char* buf, uint32_t max_len) {
    if (current_process && current_process->current_path[0] != '\0') {
        int i = 0;
        while (current_process->current_path[i] && i < (int)max_len - 1) {
            buf[i] = current_process->current_path[i];
            i++;
        }
        buf[i] = '\0';
        return;
    }
    buf[0] = '/';
    buf[1] = '\0';
}

void set_current_v3_path(const char* new_path) {
    if (current_process) {
        int i = 0;
        while (new_path[i] && i < 255) {
            current_process->current_path[i] = new_path[i];
            i++;
        }
        current_process->current_path[i] = '\0';
    }
}





// Funcție internă de comparație
static int nfs3_streq(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}


// Normalizează orice cale (relativă sau absolută) bazată pe string-uri (gestionează ., .. și /)
/*
static void nfs3_normalize_path(const char* cwd, const char* input, char* output) {
    char full[256];
    int i = 0;
    
    if (input[0] == '/') {
        full[0] = '\0';
    } else {
        while (cwd[i] && i < 200) { full[i] = cwd[i]; i++; }
        full[i] = '\0';
    }
    
    while (i > 0 && full[i - 1] != '/') {
        i--;
        full[i] = '\0';
    }
    
    // Adăugăm un slash dacă lipsește
    if (i == 0 || full[i - 1] != '/') {
        full[i++] = '/';
        full[i] = '\0';
    }
    
    int j = 0;
    while (input[j] && i < 250) {
        full[i++] = input[j++];
    }
    full[i] = '\0';
    
    // Parsăm componentele într-un stack virtual
    char comps[32][64];
    int comp_count = 0;
    
    char* token = full;
    while (*token) {
        while (*token == '/') token++;
        if (*token == '\0') break;
        
        char folder[64];
        int f = 0;
        while (*token != '/' && *token != '\0' && f < 63) {
            folder[f++] = *token++;
        }
        folder[f] = '\0';
        
        if (nfs3_streq(folder, ".")) {
            continue;
        } else if (nfs3_streq(folder, "..")) {
            if (comp_count > 0) comp_count--;
        } else {
            if (comp_count < 32) {
                int k = 0;
                while (folder[k]) { comps[comp_count][k] = folder[k]; k++; }
                comps[comp_count][k] = '\0';
                comp_count++;
            }
        }
    }
    
    // Reconstruim calea absolută curată
    output[0] = '/';
    int out_idx = 1;
    output[out_idx] = '\0';
    
    for (int c = 0; c < comp_count; c++) {
        int k = 0;
        while (comps[c][k]) {
            output[out_idx++] = comps[c][k++];
        }
        output[out_idx++] = '/';
        output[out_idx] = '\0';
    }
    
    if (out_idx > 1) {
        output[out_idx - 1] = '\0';
    }
}
*/

static void nfs3_normalize_path(const char* cwd, const char* input, char* output) {
    char full[256];
    int i = 0;
    
    // Dacă calea începe cu '/', este absolută (pleacă din root)
    if (input[0] == '/') {
        full[0] = '\0';
    } else {
        // Altfel, este relativă, deci plecăm de la CWD-ul curent
        while (cwd[i] && i < 200) { full[i] = cwd[i]; i++; }
        full[i] = '\0';
    }
    
    // Dacă input-ul nu e doar un slash, ne asigurăm că avem un '/' la sfârșitul căii de bază
    if (input[0] != '/' && (i == 0 || full[i - 1] != '/')) {
        full[i++] = '/';
        full[i] = '\0';
    }
    
    // Concatenăm input-ul la baza noastră
    int j = 0;
    while (input[j] && i < 250) {
        full[i++] = input[j++];
    }
    full[i] = '\0';
    
    // Acum procesăm folderele (eliminăm '.' și gestionăm '..')
    char comps[32][64];
    int comp_count = 0;
    
    char* token = full;
    while (*token) {
        while (*token == '/') token++;
        if (*token == '\0') break;
        
        char folder[64];
        int f = 0;
        while (*token != '/' && *token != '\0' && f < 63) {
            folder[f++] = *token++;
        }
        folder[f] = '\0';
        
        if (nfs3_streq(folder, ".")) {
            continue;
        } else if (nfs3_streq(folder, "..")) {
            if (comp_count > 0) comp_count--;
        } else {
            if (comp_count < 32) {
                int k = 0;
                while (folder[k]) { comps[comp_count][k] = folder[k]; k++; }
                comps[comp_count][k] = '\0';
                comp_count++;
            }
        }
    }
    
    // Reconstruim calea absolută finală
    output[0] = '/';
    int out_idx = 1;
    output[out_idx] = '\0';
    
    for (int c = 0; c < comp_count; c++) {
        int k = 0;
        while (comps[c][k]) {
            output[out_idx++] = comps[c][k++];
        }
        output[out_idx++] = '/';
        output[out_idx] = '\0';
    }
    
    if (out_idx > 1) {
        output[out_idx - 1] = '\0';
    }
}

// Obține inodul țintă plecând mereu de la Root (0) pe baza căii normalizate
static uint32_t nfs3_get_target_inode(const char* path) {
    if (!path || !path[0]) return 0xFFFFFFFF;
    
	char current_v3_path[256];
	get_current_v3_path(current_v3_path, sizeof(current_v3_path));
	
    char abs_path[256];
    nfs3_normalize_path(current_v3_path, path, abs_path);
    
    if (nfs3_streq(abs_path, "/")) return 0; // Root explicit
    
    uint32_t temp_inode = 0; // Începem mereu din Root
    char* token = abs_path + 1; // Trecem peste primul '/'
    
    while (*token) {
        while (*token == '/') token++;
        if (*token == '\0') break;
        
        char folder[64];
        int f = 0;
        while (*token != '/' && *token != '\0' && f < 63) {
            folder[f++] = *token++;
        }
        folder[f] = '\0';
        
        uint32_t next_inode = nfs3_find_in_dir(temp_inode, folder);
        if (next_inode == 0xFFFFFFFF) {
            return 0xFFFFFFFF; // Nu există
        }
        temp_inode = next_inode;
    }
    return temp_inode;
}

// Obține inodul țintă și raportează exact ce director lipsește dacă calea e invalidă
static uint32_t nfs3_get_target_inode_verbose(const char* path) {
    if (!path || !path[0]) return 0xFFFFFFFF;
    
	char current_v3_path[256];
	get_current_v3_path(current_v3_path, sizeof(current_v3_path));
	
    char abs_path[256];
    nfs3_normalize_path(current_v3_path, path, abs_path);
    
    if (nfs3_streq(abs_path, "/")) return 0; // Root explicit
    
    uint32_t temp_inode = 0; // Începem mereu din Root
    char* token = abs_path + 1; // Trecem peste primul '/'
    
    char partial_path[256] = "";
    
    while (*token) {
        while (*token == '/') token++;
        if (*token == '\0') break;
        
        char folder[64];
        int f = 0;
        while (*token != '/' && *token != '\0' && f < 63) {
            folder[f++] = *token++;
        }
        folder[f] = '\0';
        
        uint32_t next_inode = nfs3_find_in_dir(temp_inode, folder);
        if (next_inode == 0xFFFFFFFF) {
            // Aici șepăm exact ce nu a fost găsit!
            print("Eroare: Componenta '");
            print(folder);
            print("' nu există pe disc!\n");
            return 0xFFFFFFFF;
        }
        temp_inode = next_inode;
    }
    return temp_inode;
}

/// Rezolvă calea pentru Creare / Ștergere (separă Părintele de Numele fișierului)
static uint32_t nfs3_resolve_path(const char* full_path, char* filename_out) {
    if (!full_path || !full_path[0]) return 0xFFFFFFFF;
    
	char current_v3_path[256];
	get_current_v3_path(current_v3_path, sizeof(current_v3_path));
	
    char abs_path[256];
    nfs3_normalize_path(current_v3_path, full_path, abs_path);
    
    int last_slash = -1;
    int len = 0;
    while (abs_path[len]) {
        if (abs_path[len] == '/') last_slash = len;
        len++;
    }
    
    if (last_slash <= 0) {
        int i = 0;
        while (abs_path[i + 1]) { filename_out[i] = abs_path[i + 1]; i++; }
        filename_out[i] = '\0';
        return 0; // Părintele este Root
    }
    
    int i = 0;
    int fn_start = last_slash + 1;
    while (abs_path[fn_start + i]) {
        filename_out[i] = abs_path[fn_start + i];
        i++;
    }
    filename_out[i] = '\0';
    
    char dir_path[256];
    for (int j = 0; j < last_slash; j++) dir_path[j] = abs_path[j];
    dir_path[last_slash] = '\0';
    
    if (dir_path[0] == '\0') {
        dir_path[0] = '/';
        dir_path[1] = '\0';
    }
    
    return nfs3_get_target_inode(dir_path);
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
    set_current_dir_inode(0);
	set_current_v3_path("/");
}

void fs_list_files_v3(const char* path) {
    uint32_t target_inode = get_current_dir_inode();//current_dir_inode;

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

// Funcția CD actualizată complet
int fs_cd_v3(const char* name) {
    if (!name || name[0] == '\0') return 0;

	char current_v3_path[256];
    get_current_v3_path(current_v3_path, sizeof(current_v3_path));

    char abs_path[256];
    nfs3_normalize_path(current_v3_path, name, abs_path);
    
    //uint32_t target_inode = nfs3_get_target_inode(name);
	uint32_t target_inode = nfs3_get_target_inode_verbose(name);
    if (target_inode == 0xFFFFFFFF) return 0;
    
    if (target_inode != 0) {
        InodeV3 inv;
        nfs3_read_inode(target_inode, &inv);
        if (inv.flags != INODE_FLAG_DIR) return 0; // Nu e director
    }
    
    //current_dir_inode = target_inode;
    set_current_dir_inode(target_inode);
    // Sincronizăm calea exact cu calea absolută normalizată
	set_current_v3_path(abs_path);
	/*
    int i = 0;
    while (abs_path[i] && i < 255) {
        current_v3_path[i] = abs_path[i];
        i++;
    }
    current_v3_path[i] = '\0';
	*/
    
    return 1;
}

void fs_get_current_path_v3(char* buffer, uint32_t max_len) {
	get_current_v3_path(buffer, max_len);
	/*
    uint32_t i = 0;
	
    while (current_v3_path[i] && i < max_len - 1) {
        buffer[i] = current_v3_path[i];
        i++;
    }
    buffer[i] = '\0';
	*/
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
    set_current_dir_inode(0);
	set_current_v3_path("/");
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