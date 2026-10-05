#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define SECTOR_SIZE 512
#define NFS3_MAGIC "NAN3"

#define FAT_FREE     0x00000000
#define FAT_EOF      0xFFFFFFFF
#define FAT_BAD      0xFFFFFFFE

#define INODE_FLAG_UNUSED 0x00
#define INODE_FLAG_FILE   0x01
#define INODE_FLAG_DIR    0x02

typedef struct {
    char magic[4];
    uint32_t total_sectors;
    uint32_t fat_start_sector;
    uint32_t fat_sectors;
    uint32_t inode_start;
    uint32_t inode_count;
    uint32_t data_start;
} __attribute__((packed)) SuperblockV3;

typedef struct {
    uint32_t size;
    uint32_t first_sector;
    uint8_t  flags;
    uint8_t  reserved[3];
} __attribute__((packed)) InodeV3;

typedef struct {
    char name[64];
    uint32_t inode_index;
} __attribute__((packed)) DirEntryV3;

SuperblockV3 sb;

void read_sector(FILE* disk, uint32_t lba, uint8_t* buffer) {
    fseek(disk, lba * SECTOR_SIZE, SEEK_SET);
    fread(buffer, 1, SECTOR_SIZE, disk);
}

void write_sector(FILE* disk, uint32_t lba, uint8_t* buffer) {
    fseek(disk, lba * SECTOR_SIZE, SEEK_SET);
    fwrite(buffer, 1, SECTOR_SIZE, disk);
}

void read_inode(FILE* disk, uint32_t index, InodeV3* out) {
    // Calculăm exact ca în kernel: 42 inoduri per sector
    uint32_t inodes_per_sec = SECTOR_SIZE / sizeof(InodeV3);
    uint32_t sector_offset = index / inodes_per_sec;
    uint32_t entry_index = index % inodes_per_sec;
    
    uint32_t sector = sb.inode_start + sector_offset;
    
    uint8_t buf[SECTOR_SIZE];
    read_sector(disk, sector, buf);
    
    // Extragem inodul direct din array-ul sectorului
    InodeV3* inodes = (InodeV3*)buf;
    *out = inodes[entry_index];
}

void write_inode(FILE* disk, uint32_t index, InodeV3* in) {
    // Calculăm exact ca în kernel: 42 inoduri per sector
    uint32_t inodes_per_sec = SECTOR_SIZE / sizeof(InodeV3);
    uint32_t sector_offset = index / inodes_per_sec;
    uint32_t entry_index = index % inodes_per_sec;
    
    uint32_t sector = sb.inode_start + sector_offset;
    
    uint8_t buf[SECTOR_SIZE];
    read_sector(disk, sector, buf);
    
    // Inserăm inodul în array-ul sectorului
    InodeV3* inodes = (InodeV3*)buf;
    inodes[entry_index] = *in;
    
    // Scriem un singur sector înapoi pe disc
    write_sector(disk, sector, buf);
}

uint32_t fat_read(FILE* disk, uint32_t abs_sector) {
    uint32_t sector = sb.fat_start_sector + ((abs_sector * 4) / SECTOR_SIZE);
    uint32_t offset = (abs_sector * 4) % SECTOR_SIZE;
    uint8_t buf[SECTOR_SIZE];
    read_sector(disk, sector, buf);
    return *((uint32_t*)(buf + offset));
}

void fat_write(FILE* disk, uint32_t abs_sector, uint32_t value) {
    uint32_t sector = sb.fat_start_sector + ((abs_sector * 4) / SECTOR_SIZE);
    uint32_t offset = (abs_sector * 4) % SECTOR_SIZE;
    uint8_t buf[SECTOR_SIZE];
    read_sector(disk, sector, buf);
    *((uint32_t*)(buf + offset)) = value;
    write_sector(disk, sector, buf);
}

uint32_t allocate_fat(FILE* disk) {
    for (uint32_t i = sb.data_start; i < sb.total_sectors; i++) {
        if (fat_read(disk, i) == FAT_FREE) {
            fat_write(disk, i, FAT_EOF);
            return i; 
        }
    }
    return 0xFFFFFFFF;
}

uint32_t allocate_inode(FILE* disk) {
    for (uint32_t i = 1; i < sb.inode_count; i++) {
        InodeV3 node;
        read_inode(disk, i, &node);
        if (node.flags == INODE_FLAG_UNUSED) {
            return i;
        }
    }
    return 0xFFFFFFFF;
}

uint32_t find_inode_by_path(FILE* disk, const char* path) {
    if (!path || path[0] == '\0' || strcmp(path, "/") == 0) return 0; 

    uint32_t current_inode_idx = 0;
    char path_copy[256];
    memset(path_copy, 0, sizeof(path_copy));
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    
    char* token = strtok(path_copy, "/");
    while (token != NULL) {
        InodeV3 current_inode;
        read_inode(disk, current_inode_idx, &current_inode);
        
        if (current_inode.flags != INODE_FLAG_DIR) return 0xFFFFFFFF; 

        uint32_t current_sec = current_inode.first_sector;
        uint32_t found_idx = 0xFFFFFFFF;
        
        while (current_sec != FAT_EOF && current_sec != FAT_FREE && current_sec != 0) {
            uint8_t buf[SECTOR_SIZE];
            read_sector(disk, current_sec, buf);
            DirEntryV3* entries = (DirEntryV3*)buf;
            
            for (int i = 0; i < (int)(SECTOR_SIZE / sizeof(DirEntryV3)); i++) {
                if (entries[i].name[0] != '\0' && strcmp(entries[i].name, token) == 0) {
                    found_idx = entries[i].inode_index;
                    break;
                }
            }
            if (found_idx != 0xFFFFFFFF) break;
            current_sec = fat_read(disk, current_sec);
        }
        
        if (found_idx == 0xFFFFFFFF) return 0xFFFFFFFF; 
        current_inode_idx = found_idx;
        token = strtok(NULL, "/");
    }
    return current_inode_idx;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Utilizare nan3hdd:\n");
        printf("  %s <disk.img> ls [cale]\n", argv[0]);
        printf("  %s <disk.img> mkdir <cale_noua>\n", argv[0]);
        printf("  %s <disk.img> push <fisier_linux> <cale_in_nano>\n", argv[0]);
        printf("  %s <disk.img> get <cale_in_nano> <fisier_linux>\n", argv[0]);
        printf("  %s <disk.img> format\n", argv[0]);
        return 1;
    }

    int saved = 0;
    const char* img_path = argv[1];
    const char* command = argv[2];
    FILE* disk = fopen(img_path, "r+b");
    if (!disk) { printf("Eroare la deschiderea imaginii!\n"); return 1; }

    uint8_t root_buf[SECTOR_SIZE];
    read_sector(disk, 1, root_buf);
    memcpy(&sb, root_buf, sizeof(SuperblockV3));

    // --- COMANDA LS ---
    if (strcmp(command, "ls") == 0) {
        uint32_t target_inode_idx = 0; 
        if (argc >= 4) {
            target_inode_idx = find_inode_by_path(disk, argv[3]);
            if (target_inode_idx == 0xFFFFFFFF) {
                printf("Eroare: Calea '%s' nu a fost gasita.\n", argv[3]);
                fclose(disk); return 1;
            }
        }
        
        InodeV3 target_inode;
        read_inode(disk, target_inode_idx, &target_inode);
        
        printf("--- Conținut Director (Inode %d) ---\n", target_inode_idx);
        uint32_t current_sec = target_inode.first_sector;
        
        while (current_sec != FAT_EOF && current_sec != FAT_FREE && current_sec != 0) {
            uint8_t buf[SECTOR_SIZE];
            read_sector(disk, current_sec, buf);
            DirEntryV3* entries = (DirEntryV3*)buf;
            
            for (int i = 0; i < (int)(SECTOR_SIZE / sizeof(DirEntryV3)); i++) {
                if (entries[i].name[0] != '\0') {
                    InodeV3 child;
                    read_inode(disk, entries[i].inode_index, &child);
                    printf("%s %s - %d bytes\n", 
                        (child.flags == INODE_FLAG_DIR) ? "[DIR] " : "[FILE]", 
                        entries[i].name, child.size);
                }
            }
            current_sec = fat_read(disk, current_sec);
        }
    }
    // --- COMANDA MKDIR ---
    else if (strcmp(command, "mkdir") == 0 && argc == 4) {
        char path_copy[256];
        memset(path_copy, 0, sizeof(path_copy));
        strncpy(path_copy, argv[3], sizeof(path_copy) - 1);
        
        char* last_slash = strrchr(path_copy, '/');
        char dir_path[256] = "/";
        const char* new_dir_name = argv[3];

        if (last_slash != NULL) {
            if (last_slash == path_copy) dir_path[1] = '\0';
            else { 
                *last_slash = '\0'; 
                memset(dir_path, 0, sizeof(dir_path));
                strncpy(dir_path, path_copy, sizeof(dir_path) - 1);                
            }
            new_dir_name = last_slash + 1;
        }

        uint32_t parent_inode_idx = find_inode_by_path(disk, dir_path);
        if (parent_inode_idx == 0xFFFFFFFF) { printf("Director parinte inexistent!\n"); fclose(disk); return 1; }

        uint32_t new_inode_idx = allocate_inode(disk);
        uint32_t new_data_idx = allocate_fat(disk);
        
        InodeV3 new_inode = {0};
        new_inode.flags = INODE_FLAG_DIR;
        new_inode.first_sector = new_data_idx;
        new_inode.size = 0;
        write_inode(disk, new_inode_idx, &new_inode);

        uint8_t zero_buf[SECTOR_SIZE] = {0};
        write_sector(disk, new_data_idx, zero_buf);
        
        InodeV3 parent_inode;
        read_inode(disk, parent_inode_idx, &parent_inode);
        uint32_t current_sec = parent_inode.first_sector;
        
        while (current_sec != FAT_EOF && !saved) {
            uint8_t buf[SECTOR_SIZE];
            read_sector(disk, current_sec, buf);
            DirEntryV3* entries = (DirEntryV3*)buf;
            for (int i = 0; i < (int)(SECTOR_SIZE / sizeof(DirEntryV3)); i++) {
                if (entries[i].name[0] == '\0') {
                    memset(entries[i].name, 0, sizeof(entries[i].name));
                    strncpy(entries[i].name, new_dir_name, sizeof(entries[i].name) - 1);
                    
                    entries[i].inode_index = new_inode_idx;
                    write_sector(disk, current_sec, buf);
                    saved = 1; break;
                }
            }
            if (!saved) current_sec = fat_read(disk, current_sec);
        }
        printf("Directorul '%s' a fost creat.\n", argv[3]);
    }
    // --- COMANDA PUSH ---
    else if (strcmp(command, "push") == 0 && argc == 5) {
        char path_copy[256];
        memset(path_copy, 0, sizeof(path_copy));
        strncpy(path_copy, argv[4], sizeof(path_copy) - 1);
        
        char* last_slash = strrchr(path_copy, '/');
        char dir_path[256] = "/";
        const char* file_name = argv[4];

        if (last_slash != NULL) {
            if (last_slash == path_copy) {
                dir_path[1] = '\0';
            } else { 
                *last_slash = '\0'; 
                memset(dir_path, 0, sizeof(dir_path));
                strncpy(dir_path, path_copy, sizeof(dir_path) - 1);
            }
            file_name = last_slash + 1;
        }

        // --- SANITIZARE NUME FIȘIER ---
        char clean_file_name[64];
        memset(clean_file_name, 0, sizeof(clean_file_name));
        strncpy(clean_file_name, file_name, 63);
        
        for (int j = 0; j < 64; j++) {
            if (clean_file_name[j] == '\r' || clean_file_name[j] == '\n' || 
                clean_file_name[j] == '\t' || clean_file_name[j] == ' ') {
                clean_file_name[j] = '\0';
                break; 
            }
        }
        // ------------------------------

        uint32_t parent_inode_idx = find_inode_by_path(disk, dir_path);
        if (parent_inode_idx == 0xFFFFFFFF) { 
            printf("Director parinte inexistent!\n"); 
            fclose(disk);
            return 1; 
        }

        FILE* local_f = fopen(argv[3], "rb");
        if (!local_f) { 
            printf("Nu pot citi fisierul sursa\n"); 
            fclose(disk);
            return 1; 
        }
        fseek(local_f, 0, SEEK_END);
        uint32_t size = ftell(local_f);
        fseek(local_f, 0, SEEK_SET);

        uint32_t sectors_needed = size > 0 ? (size + SECTOR_SIZE - 1) / SECTOR_SIZE : 1;
        uint32_t new_inode_idx = allocate_inode(disk);
        uint32_t start_sector = allocate_fat(disk);

        InodeV3 new_inode = {0};
        new_inode.flags = INODE_FLAG_FILE;
        new_inode.first_sector = start_sector;
        new_inode.size = size;
        write_inode(disk, new_inode_idx, &new_inode);

        uint32_t current_sec = start_sector;
        uint8_t temp_buf[SECTOR_SIZE];
        for (uint32_t i = 0; i < sectors_needed; i++) {
            memset(temp_buf, 0, SECTOR_SIZE);
            uint32_t to_read = (size - i * SECTOR_SIZE < SECTOR_SIZE) ? (size - i * SECTOR_SIZE) : SECTOR_SIZE;
            fread(temp_buf, 1, to_read, local_f);
            write_sector(disk, current_sec, temp_buf);

            if (i < sectors_needed - 1) {
                uint32_t next_sec = allocate_fat(disk);
                fat_write(disk, current_sec, next_sec);
                current_sec = next_sec;
            }
        }

        InodeV3 parent_inode;
        read_inode(disk, parent_inode_idx, &parent_inode);
        current_sec = parent_inode.first_sector;
        
        uint32_t last_dir_sec = current_sec;
        saved = 0;
        
        while (current_sec != FAT_EOF && current_sec != FAT_FREE && current_sec != 0 && !saved) {
            last_dir_sec = current_sec;
            uint8_t buf[SECTOR_SIZE];
            read_sector(disk, current_sec, buf);
            DirEntryV3* entries = (DirEntryV3*)buf;
            
            for (int i = 0; i < (int)(SECTOR_SIZE / sizeof(DirEntryV3)); i++) {
                if (entries[i].name[0] == '\0') {
                    memset(entries[i].name, 0, sizeof(entries[i].name));
                    strncpy(entries[i].name, clean_file_name, sizeof(entries[i].name) - 1);
                    
                    entries[i].inode_index = new_inode_idx;
                    write_sector(disk, current_sec, buf);
                    saved = 1; 
                    break;
                }
            }
            if (!saved) {
                current_sec = fat_read(disk, current_sec);
            }
        }
        
        if (!saved && last_dir_sec != FAT_EOF && last_dir_sec != 0) {
            uint32_t new_dir_sec = allocate_fat(disk);
            if (new_dir_sec != 0xFFFFFFFF) {
                fat_write(disk, last_dir_sec, new_dir_sec); 
                
                uint8_t buf[SECTOR_SIZE];
                memset(buf, 0, SECTOR_SIZE);
                DirEntryV3* entries = (DirEntryV3*)buf;
                
                strncpy(entries[0].name, clean_file_name, sizeof(entries[0].name) - 1);
                entries[0].inode_index = new_inode_idx;
                write_sector(disk, new_dir_sec, buf);
                saved = 1;
            } else {
                printf("Eroare: Nu mai exista spatiu pe disc pentru a extinde directorul!\n");
            }
        }
        
        if (saved) {
            printf("Fisierul '%s' a fost salvat in '%s'.\n", argv[3], dir_path);
        } else {
            printf("Eroare critica: Fisierul nu a putut fi adaugat in director.\n");
        }
        
        fclose(local_f);
    }
    // --- COMANDA FORMAT ---
    else if (strcmp(command, "format") == 0) {
        uint32_t total_sec = 102400; 
        if (argc >= 4) {
            total_sec = (atoi(argv[3]) * 1024) / SECTOR_SIZE;
        }

        memcpy(sb.magic, NFS3_MAGIC, 4);
        sb.total_sectors = total_sec;
        sb.fat_start_sector = 2;
        sb.fat_sectors = 200;        
        sb.inode_start = 202;
        sb.inode_count = 1000;      
        sb.data_start = 1226;        

        uint8_t buf[SECTOR_SIZE] = {0};
        memcpy(buf, &sb, sizeof(SuperblockV3));
        write_sector(disk, 1, buf);

        memset(buf, 0, SECTOR_SIZE);
        for (uint32_t i = 0; i < sb.fat_sectors; i++) {
            write_sector(disk, sb.fat_start_sector + i, buf);
        }

        for (uint32_t i = 0; i < sb.data_start; i++) {
            fat_write(disk, i, FAT_BAD);
        }

        uint32_t inode_sectors = (sb.inode_count * sizeof(InodeV3) + 511) / SECTOR_SIZE;
        memset(buf, 0, SECTOR_SIZE);
        for (uint32_t i = 0; i < inode_sectors; i++) {
            write_sector(disk, sb.inode_start + i, buf);
        }

        InodeV3 root_inode = {0};
        root_inode.flags = INODE_FLAG_DIR;
        root_inode.size = 0;
        
        uint32_t root_data_sec = allocate_fat(disk); 
        root_inode.first_sector = root_data_sec;
        write_inode(disk, 0, &root_inode);

        memset(buf, 0, SECTOR_SIZE);
        write_sector(disk, root_data_sec, buf);

        printf("Succes: Formatare NanoFS V3 completă (%d sectoare).\n", total_sec);
        fclose(disk);
        return 0;
    }
    // --- COMANDA GET ---
    else if (strcmp(command, "get") == 0 && argc == 5) {
        const char* nano_file = argv[3];
        const char* linux_file = argv[4];

        uint32_t target_inode_idx = find_inode_by_path(disk, nano_file);
        
        if (target_inode_idx == 0xFFFFFFFF) {
            printf("Eroare: Fisierul '%s' nu a fost gasit in NanoFS.\n", nano_file);
        } else {
            InodeV3 file_inode;
            read_inode(disk, target_inode_idx, &file_inode);

            if (file_inode.flags != INODE_FLAG_FILE) {
                printf("Eroare: '%s' nu este un fisier!\n", nano_file);
            } else {
                FILE* local_f = fopen(linux_file, "wb");
                if (!local_f) {
                    printf("Eroare: Nu pot crea fisierul local '%s'\n", linux_file);
                } else {
                    uint32_t bytes_left = file_inode.size;
                    uint32_t current_sec = file_inode.first_sector;

                    while (bytes_left > 0 && current_sec != FAT_EOF && current_sec != FAT_FREE && current_sec != 0) {
                        uint8_t buf[SECTOR_SIZE];
                        read_sector(disk, current_sec, buf); 
                        
                        uint32_t to_write = (bytes_left > SECTOR_SIZE) ? SECTOR_SIZE : bytes_left;
                        fwrite(buf, 1, to_write, local_f);
                        
                        bytes_left -= to_write;
                        current_sec = fat_read(disk, current_sec);
                    }

                    printf("Succes: Fisierul '%s' a fost extras in '%s' (%d bytes).\n", nano_file, linux_file, file_inode.size);
                    fclose(local_f);
                }
            }
        }
    }
    else {
        printf("Comandă necunoscută: '%s'\n", command);
    }
    
    fclose(disk);
    return 0;
}