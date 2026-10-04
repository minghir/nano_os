#include "fs.h"
#include "../syslog.h"
#include "../string.h"
#include "../io.h"

// Presupunem că funcția streq este definită aici sau inclusă dintr-un header utilitar
static int streq(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

// Declarăm cele două drivere (NAN2 și NanoFS V3)
extern FileSystemInterface nan2_driver;
extern FileSystemInterface nfs3_driver;

// Pointerul către sistemul de fișiere activ în acest moment
static FileSystemInterface* active_fs = &nan2_driver;
static int is_at_global_root = 1;

// Funcție utilitară pentru a verifica dacă un string începe cu un prefix
static int starts_with_vfs(const char* str, const char* prefix) {
    while (*prefix) {
        if (*prefix++ != *str++) return 0;
    }
    return 1;
}

// Router VFS: Determină driverul și calea locală pe baza prefixului
// Router VFS: Extrage calea reală și returnează driverul corespunzător
FileSystemInterface* vfs_route(const char* full_path, char* local_path_out) {
    if (!full_path || full_path[0] == '\0') {
        local_path_out[0] = '\0';
        return active_fs;
    }

    // Ruta explicită către hda (/hda sau /hda/...)
    if (starts_with_vfs(full_path, "/hda")) {
        int i = 4; // Trecem peste "/hda"
        int j = 0;
        if (full_path[i] == '\0') {
            local_path_out[0] = '/';
            local_path_out[1] = '\0';
        } else {
            while (full_path[i]) local_path_out[j++] = full_path[i++];
            local_path_out[j] = '\0';
        }
        return &nan2_driver;
    }
    
    // Ruta explicită către hdb (/hdb sau /hdb/...)
    if (starts_with_vfs(full_path, "/hdb")) {
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

    // Dacă suntem la root-ul global și se cere o cale simplă care nu începe cu /hda sau /hdb
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
    if (streq(name, "v3") || streq(name, "NAN3") || streq(name, "2") || streq(name, "hdb")) {
        active_fs = &nfs3_driver;
        active_fs->init();
        KLOG_INFO("Switched to NanoFS V3 driver (hdb).\n");
    } else {
        active_fs = &nan2_driver;
        active_fs->init();
        KLOG_INFO("Switched to NAN2 driver (hda).\n");
    }
}

// Funcții wrapper unificate (Dispatcher VFS)
void fs_init(void) {
    // 1. Pornim și inițializăm ambele drivere la pornirea sistemului!
    if (nan2_driver.init) nan2_driver.init();
    if (nfs3_driver.init) nfs3_driver.init();
    
    // 2. Setăm mediul implicit (Rădăcina Virtuală Globală)
    active_fs = &nan2_driver; 
    is_at_global_root = 1;
}

// 1. Listarea fișierelor
void fs_list_files(const char* path) {
    // Dacă suntem la rădăcina globală și path-ul este gol sau "/"
    if (is_at_global_root && (!path || path[0] == '\0' || streq(path, "/") || streq(path, "."))) {
        print("VFS Root Directory Contents:\n");
        print("[DIR]  hda  (NAN2 - Primary Disk)\n");
        print("[DIR]  hdb  (NanoFS V3 - 50MB Disk)\n");
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

    // Dacă suntem la rădăcina globală și utilizatorul încearcă să creeze ceva 
    // direct la rădăcină (ex: touch fisier.txt) fără să specifice /hda sau /hdb,
    // îl refuzăm sau îl dirijăm către discul activ. Cel mai elegant este să 
    // lăsăm discul activ să încerce (sau să returnăm eroare că nu se poate scrie în VFS root).
    if (is_at_global_root && name[0] != '/') {
        // Opțional: poți alege să creezi pe discul activ curent:
        // return active_fs->create_file(name, size);
        return 0; // Nu putem scrie direct în VFS Root-ul virtual global
    }

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

// 2. Schimbarea directorului (CD) cu suport pentru VFS Global
int fs_cd(const char* name) {
    if (!name || name[0] == '\0') return 0;

    // Revenirea la rădăcina globală "/"
    if (streq(name, "/") || streq(name, "/.")) {
        is_at_global_root = 1;
        return 1;
    }

    // Dacă suntem la rădăcina globală și vrem să intrăm în hda sau hdb
    if (is_at_global_root) {
        if (streq(name, "hda") || streq(name, "/hda")) {
            active_fs = &nan2_driver;
            // ATENȚIE: Nu apelăm active_fs->init() aici, ci doar cd("/") sau lăsăm starea intactă
            active_fs->cd("/");
            is_at_global_root = 0;
            return 1;
        }
        if (streq(name, "hdb") || streq(name, "/hdb")) {
            active_fs = &nfs3_driver;
            // ATENȚIE: Fără init() aici! Păstrăm starea intactă a discului hdb.
            active_fs->cd("/");
            is_at_global_root = 0;
            return 1;
        }
    }

    // Gestionarea lui ".." când suntem imediat sub root-ul global
    if (streq(name, "..") && !is_at_global_root) {
        char current_sub_path[64];
        active_fs->get_current_path(current_sub_path, 64);
        if (streq(current_sub_path, "/")) {
            is_at_global_root = 1;
            return 1;
        }
    }

    // Rutare normală pentru subdirectoare
    char local_path[128];
    FileSystemInterface* target = vfs_route(name, local_path);
    if (target != active_fs) {
        active_fs = target;
        is_at_global_root = 0;
    }
    
    if (target && target->cd) {
        return target->cd(local_path);
    }
    
    return 0;
}

// 3. Returnarea căii curente pentru prompt (PWD / getcwd)
void fs_get_current_path(char* buffer, uint32_t max_len) {
    if (is_at_global_root) {
        buffer[0] = '/';
        buffer[1] = '\0';
        return;
    }

    char sub_path[64];
    active_fs->get_current_path(sub_path, 64);

    // Construim calea completă prefixată cu /hda sau /hdb
    const char* prefix = (active_fs == &nan2_driver) ? "/hda" : "/hdb";
    
    int i = 0;
    while (prefix[i] && i < max_len - 1) {
        buffer[i] = prefix[i];
        i++;
    }

    // Dacă sub_path nu este doar "/", îl adăugăm
    if (!streq(sub_path, "/")) {
        int j = 0;
        while (sub_path[j] && i < max_len - 1) {
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