#include "fs.h"
#include "../syslog.h"
#include "../string.h"
#include "../io.h"


uint8_t current_drive_id = 0;

static int streq(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

// Declarăm driverul NanoFS V3 (acum singurul suportat)
extern FileSystemInterface nfs3_driver;

// Pointerul către sistemul de fișiere activ în acest moment
static FileSystemInterface* active_fs = &nfs3_driver;
static int is_at_global_root = 1;

static int starts_with_vfs(const char* str, const char* prefix) {
    while (*prefix) {
        if (*prefix++ != *str++) return 0;
    }
    return 1;
}

// Router VFS: Extrage calea reală și returnează driverul V3
FileSystemInterface* vfs_route(const char* full_path, char* local_path_out) {
    if (!full_path || full_path[0] == '\0') {
        local_path_out[0] = '\0';
        return active_fs;
    }

    // Ruta explicită către hda (/hda sau /hda/...) -> Folosește tot NanoFS V3
    if (starts_with_vfs(full_path, "/hda")) {
		current_drive_id = 0;
        int i = 4; // Trecem peste "/hda"
        int j = 0;
        if (full_path[i] == '\0') {
            local_path_out[0] = '/';
            local_path_out[1] = '\0';
        } else {
            while (full_path[i]) local_path_out[j++] = full_path[i++];
            local_path_out[j] = '\0';
        }
        return &nfs3_driver;
    }
    
    // Ruta explicită către hdb (/hdb sau /hdb/...) -> Folosește NanoFS V3
    if (starts_with_vfs(full_path, "/hdb")) {
		current_drive_id = 1;
        int i = 4; // Trecem peste "/hdb"
        int j = 0;
        if (full_path[i] == '\0') {
            local_path_out[0] = '/';
            local_path_out[1] = '\0';
        } else {
            while (full_path[i]) local_path_out[j++] = full_path[i++];
            local_path_out[j] = '\0';
        }
        return &nfs3_driver;
    }

    int i = 0;
    while (full_path[i]) {
        local_path_out[i] = full_path[i];
        i++;
    }
    local_path_out[i] = '\0';
    
    return active_fs;
}

void fs_mount_driver(FileSystemInterface* driver) {
    active_fs = driver;
}

void fs_switch_driver(const char* name) {
    (void)name;
    active_fs = &nfs3_driver;
    active_fs->init();
    KLOG_INFO("Switched to NanoFS V3 driver.\n");
}

void fs_init(void) {
    // Inițializăm motorul V3
    if (nfs3_driver.init) nfs3_driver.init();
    
    active_fs = &nfs3_driver; 
    is_at_global_root = 1;
}

void fs_list_files(const char* path) {
    if (is_at_global_root && (!path || path[0] == '\0' || streq(path, "/") || streq(path, "."))) {
        print("VFS Root Directory Contents:\n");
        print("[DIR]  hda  (NanoFS V3 - Disk 1)\n");
        print("[DIR]  hdb  (NanoFS V3 - Disk 2)\n");
        return;
    }

    char local_path[128];
    FileSystemInterface* target = vfs_route(path, local_path);
    if (target && target->list_files) {
        target->list_files(local_path);
    }
}

int fs_read_file(const char* name, uint8_t* buffer, uint32_t max_size) {
    char local_path[128];
    FileSystemInterface* target = vfs_route(name, local_path);
    if (target && target->read_file) return target->read_file(local_path, buffer, max_size);
    return 0;
}

int fs_create_file(const char* name, uint32_t size) {
    if (!name || name[0] == '\0') return 0;
    if (is_at_global_root && name[0] != '/') return 0;

    char local_path[128];
    FileSystemInterface* target = vfs_route(name, local_path);
    if (target && target->create_file) {
        return target->create_file(local_path, size);
    }
    return 0;
}

int fs_write_file(const char* name, const uint8_t* data, uint32_t size) {
    char local_path[128];
    FileSystemInterface* target = vfs_route(name, local_path);
    if (target && target->write_file) return target->write_file(local_path, data, size);
    return 0;
}

int fs_delete_file(const char* name) {
    char local_path[128];
    FileSystemInterface* target = vfs_route(name, local_path);
    if (target && target->delete_file) return target->delete_file(local_path);
    return 0;
}

int fs_mkdir(const char* name) {
    char local_path[128];
    FileSystemInterface* target = vfs_route(name, local_path);
    if (target && target->mkdir) return target->mkdir(local_path);
    return 0;
}

int fs_cd(const char* name) {
    if (!name || name[0] == '\0') return 0;

    // Suntem direcționați spre root-ul global
    if (streq(name, "/") || streq(name, "/.")) {
        is_at_global_root = 1;
        return 1;
    }

    if (is_at_global_root) {
        if (streq(name, "hda") || streq(name, "/hda")) {
            current_drive_id = 0;
            active_fs = &nfs3_driver;
            active_fs->cd("/");
            is_at_global_root = 0;
            return 1;
        }
        if (streq(name, "hdb") || streq(name, "/hdb")) {
            current_drive_id = 1;
            active_fs = &nfs3_driver;
            active_fs->cd("/");
            is_at_global_root = 0;
            return 1;
        }
    }

    if (streq(name, "..") && !is_at_global_root) {
        char current_sub_path[64];
        active_fs->get_current_path(current_sub_path, 64);
        if (streq(current_sub_path, "/")) {
            is_at_global_root = 1;
            return 1;
        }
    }

    char local_path[128];
    FileSystemInterface* target = vfs_route(name, local_path);
    
    if (target && target->cd) {
        int success = target->cd(local_path);
        if (success) {
            active_fs = target;
            // FIX CRITIC: Dacă am intrat cu succes într-un path din driver, NU mai suntem în global root!
            is_at_global_root = 0; 
            return 1;
        }
    }
    
    return 0;
}

void fs_get_current_path(char* buffer, uint32_t max_len) {
    if (is_at_global_root) {
        buffer[0] = '/';
        buffer[1] = '\0';
        return;
    }

    char sub_path[64];
    active_fs->get_current_path(sub_path, 64);

    // Verificăm dacă suntem pe hda sau hdb în funcție de starea curentă
    // (Poți personaliza prefixul dacă ai două instanțe separate de driver)
    const char* prefix = (current_drive_id == 0) ? "/hda" : "/hdb";
    
    int i = 0;
    while (prefix[i] && i < (int)max_len - 1) {
        buffer[i] = prefix[i];
        i++;
    }

    if (!streq(sub_path, "/")) {
        int j = 0;
        while (sub_path[j] && i < (int)max_len - 1) {
            buffer[i++] = sub_path[j++];
        }
    }
    buffer[i] = '\0';
}

void fs_fdisk(void) {
    if (active_fs && active_fs->fdisk) active_fs->fdisk();
}

void fs_format(void) {
    if (active_fs && active_fs->format) active_fs->format();
}

int fs_get_file_at_index(int index, char* buffer, uint32_t max_len) {
    if (active_fs && active_fs->get_file_at_index) {
        return active_fs->get_file_at_index(index, buffer, max_len);
    }
    return 0;
}