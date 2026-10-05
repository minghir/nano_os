#ifndef FS_H
#define FS_H

#include <stdint.h>



extern uint8_t current_drive_id;

// Structura pentru interfața virtuală (VFS)
typedef struct {
    const char* name;
	int (*get_stats)(uint32_t* total_sectors, uint32_t* free_sectors);
    void (*init)(void);
    void (*list_files)(const char* path);
    int  (*create_file)(const char* name, uint32_t size);
    int  (*delete_file)(const char* name);
    int  (*write_file)(const char* name, const uint8_t* data, uint32_t size);
    int  (*read_file)(const char* name, uint8_t* buffer, uint32_t max_size);
    int  (*mkdir)(const char* name);
    int  (*rmdir)(const char* name);
    int  (*cd)(const char* name);
    void (*get_current_path)(char* buffer, uint32_t max_len);
    void (*fdisk)(void);
    void (*format)(void);
	int  (*get_file_at_index)(int index, char* buffer, uint32_t max_len);
} FileSystemInterface;

// --- API-ul public unificat (Dispatcher-ul) ---
void fs_switch_driver(const char* name);
FileSystemInterface* vfs_route(const char* full_path, char* local_path_out);

int  fs_stats(uint32_t* total_sectors, uint32_t* free_sectors);
void fs_init(void);
void fs_list_files(const char* path);
int  fs_create_file(const char* name, uint32_t size);
int  fs_delete_file(const char* name);
int  fs_write_file(const char* name, const uint8_t* data, uint32_t size);
int  fs_read_file(const char* name, uint8_t* buffer, uint32_t max_size);
int  fs_mkdir(const char* name);
int  fs_rmdir(const char* name);
int  fs_cd(const char* name);
void fs_get_current_path(char* buffer, uint32_t max_len);
void fs_fdisk(void);
void fs_format(void);
int  fs_get_file_at_index(int index, char* buffer, uint32_t max_len);
// Permitem schimbarea sau exportul driverului NAN2
extern FileSystemInterface nan2_driver;

#endif