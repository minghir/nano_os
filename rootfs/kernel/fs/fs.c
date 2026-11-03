#include "fs.h"
#include "../syslog.h"
#include "../string.h"
#include "../process.h"
#include "../io.h"
#include "../pcb.h"


// Structura pentru tabela de montare (fstab-ul nostru din memorie)
typedef struct {
    char mount_point[16];     // ex: "/hdb"
    int drive_id;             // ex: 1 (Primary Slave)
    FileSystemInterface* fs;  // pointer către ext2_driver sau nfs3_driver
    int in_use;
} MountEntry;


#define MAX_MOUNTS 4
MountEntry mount_table[MAX_MOUNTS];


uint8_t current_drive_id = 0;

static int streq(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

// Declarăm driverul NanoFS V3 (acum singurul suportat)
extern FileSystemInterface nfs3_driver;
extern FileSystemInterface ext2_driver;

// Pointerul către sistemul de fișiere activ în acest moment
static FileSystemInterface* active_fs = &nfs3_driver;
static int is_at_global_root = 1;

static int starts_with_vfs(const char* str, const char* prefix) {
    while (*prefix) {
        if (*prefix++ != *str++) return 0;
    }
    // Trebuie să se termine prefixul, și următorul caracter din path trebuie să fie '/' sau null
    // ex: prefixul e "/hdb", path-ul poate fi "/hdb" sau "/hdb/folder", dar nu "/hdb_test"
    if (*str == '\0' || *str == '/') return 1;
    return 0;
}

extern void get_cmdline_param(const char* cmdline, const char* param_name, char* out_val, int max_len);

// --- Funcția Principală de Inițializare VFS ---
// Această funcție primește linia de comandă salvată de la Multiboot
void fs_init(const char* kernel_cmdline) {
    char rootfs_type[16] = {0};
    
    // Curățăm tabela VFS la pornire
    for (int i = 0; i < MAX_MOUNTS; i++) {
        mount_table[i].in_use = 0;
        mount_table[i].drive_id = 0;
        mount_table[i].mount_point[0] = '\0';
        mount_table[i].fs = 0;
    }

    // 1. Extragem tipul partiției root din cmdline (fallback "nfs3")
    get_cmdline_param(kernel_cmdline, "rootfs=", rootfs_type, sizeof(rootfs_type));
    if (rootfs_type[0] == '\0') {
        string_copy(rootfs_type, "nfs3"); 
    }

    // 2. Inițializăm temporar driverul pentru a citi hdd.cfg
    KLOG_INFO("Boot VFS Init: Montare temporara Drive 0 pentru citire config...\n");
    current_drive_id = 0;
    if (string_compare(rootfs_type, "nfs3") == 0) {
        nfs3_driver.init();
        active_fs = &nfs3_driver;
    } else if (string_compare(rootfs_type, "ext2") == 0) {
        ext2_driver.init();
        active_fs = &ext2_driver;
    } else {
        KLOG_ERROR("Eroare fatala: Tip rootfs necunoscut!");
        return;
    }
    is_at_global_root = 1;

    // 3. Citim `hdd.cfg` direct de pe discul abia inițializat
    uint8_t cfg_buffer[1024];
    int bytes_read = active_fs->read_file("/cfg/hdd.cfg", cfg_buffer, 1023);

    if (bytes_read > 0) {
        cfg_buffer[bytes_read] = '\0';
        KLOG_INFO("Se populeaza tabela VFS din /cfg/hdd.cfg...\n");
        
        char* ptr = (char*)cfg_buffer;
        while (*ptr != '\0') {
            while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r') ptr++;
            if (*ptr == '\0') break;
            
            if (*ptr == '#') {
                while (*ptr != '\n' && *ptr != '\0') ptr++;
                continue;
            }
            
            // Parsăm coloana 1: DriveID
            char drive_str[16];
            int idx = 0;
            while (*ptr != ' ' && *ptr != '\t' && *ptr != '\n' && *ptr != '\0' && idx < 15) {
                drive_str[idx++] = *ptr++;
            }
            drive_str[idx] = '\0';
            
            while (*ptr == ' ' || *ptr == '\t') ptr++;
            
            // Parsăm coloana 2: FSType
            char fs_str[16];
            idx = 0;
            while (*ptr != ' ' && *ptr != '\t' && *ptr != '\n' && *ptr != '\0' && idx < 15) {
                fs_str[idx++] = *ptr++;
            }
            fs_str[idx] = '\0';
            
            while (*ptr == ' ' || *ptr == '\t') ptr++;
            
            // Parsăm coloana 3: MountPoint
            char mp_str[32];
            idx = 0;
            while (*ptr != ' ' && *ptr != '\t' && *ptr != '\n' && *ptr != '\r' && *ptr != '\0' && idx < 31) {
                mp_str[idx++] = *ptr++;
            }
            mp_str[idx] = '\0';
            
            while (*ptr != '\n' && *ptr != '\0') ptr++;
            
            // Înregistrăm discul în tabela de montare
            if (fs_str[0] != '\0' && mp_str[0] != '\0') {
                int parsed_drive_id = simple_atoi(drive_str);
                
                int free_slot = -1;
                for (int m = 0; m < MAX_MOUNTS; m++) {
                    if (!mount_table[m].in_use) {
                        free_slot = m;
                        break;
                    }
                }
                
                if (free_slot != -1) {
                    mount_table[free_slot].drive_id = parsed_drive_id;
                    string_copy(mount_table[free_slot].mount_point, mp_str);
                    
                    if (string_compare(fs_str, "ext2") == 0) {
                        mount_table[free_slot].fs = &ext2_driver;
                        
                        // Setăm drive-ul pentru inițializare hardware
                        uint8_t saved_drive = current_drive_id;
                        current_drive_id = parsed_drive_id;
                        ext2_driver.init();
                        current_drive_id = saved_drive;
                        
                    } else if (string_compare(fs_str, "nfs3") == 0) {
                        mount_table[free_slot].fs = &nfs3_driver;
                        
                        // DACĂ NU ESTE ROOT-ul, trebuie inițializat separat (dacă motorul tău o cere).
                        // Root-ul a fost deja inițializat mai sus.
                        if (parsed_drive_id != 0) {
                            uint8_t saved_drive = current_drive_id;
                            current_drive_id = parsed_drive_id;
                            // nfs3_driver.init(); // Decomentează dacă NanoFS are nevoie de init explicit per drive
                            current_drive_id = saved_drive;
                        }
                    } else {
                        KLOG_ERROR("Eroare: Tip FS necunoscut!\n");
                        continue;
                    }

                    mount_table[free_slot].in_use = 1;
                    
                    char log_msg[128];
                    snprintf(log_msg, sizeof(log_msg), " -> Montat: Drive %d [%s] la %s\n", parsed_drive_id, fs_str, mp_str);
                    KLOG_INFO(log_msg);
                    
                } else {
                    KLOG_ERROR("Eroare: Tabela VFS este plina.\n");
                }
            }
        }
    } else {
        KLOG_INFO("Avertisment: Nu s-a gasit /cfg/hdd.cfg. Sistem partial functional.\n");
    }
}



// Router VFS: Extrage calea reală și returnează driverul V3
// Memorează ultimul disc accesat hardware pentru a preveni coruperea în RAM
// Memorează ultimul disc accesat hardware pentru a preveni coruperea în RAM
static uint8_t active_hardware_drive = 255;

FileSystemInterface* vfs_route(const char* full_path, char* local_path_out) {
    if (!full_path || full_path[0] == '\0') {
        local_path_out[0] = '\0';
        goto fallback;
    }

    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (!mount_table[i].in_use) continue;

        if (starts_with_vfs(full_path, mount_table[i].mount_point)) {
            current_drive_id = mount_table[i].drive_id; 
            
            if (active_hardware_drive != current_drive_id) {
                if (mount_table[i].fs->init) mount_table[i].fs->init();
                if (mount_table[i].fs->cd) mount_table[i].fs->cd("/");
                active_hardware_drive = current_drive_id;
            }

            int prefix_len = string_length(mount_table[i].mount_point);
            int k = prefix_len;
            int j = 0;
            
            if (full_path[k] == '\0') {
                local_path_out[0] = '/';
                local_path_out[1] = '\0';
            } else {
                while (full_path[k]) local_path_out[j++] = full_path[k++];
                local_path_out[j] = '\0';
            }
            
            return mount_table[i].fs; 
        }
    }

    // Dacă path-ul nu e absolut (ex: "DOOM1.WAD" sau "bin"), copiem stringul pur
    int i = 0;
    while (full_path[i]) {
        local_path_out[i] = full_path[i];
        i++;
    }
    local_path_out[i] = '\0';

fallback:
    // --- FIX CRITIC PENTRU ATA ERRORS ---
    // Dacă am ajuns aici, VFS-ul trimite comanda către driverul "active_fs" anterior.
    // Ne asigurăm absolut că driverul fizic (current_drive_id) corespunde cu active_fs,
    // altfel driverul EXT2 ar putea încerca să citească din greșeală de pe discul NanoFS!
    for (int m = 0; m < MAX_MOUNTS; m++) {
        if (mount_table[m].in_use && mount_table[m].fs == active_fs) {
            current_drive_id = mount_table[m].drive_id;
            
            if (active_hardware_drive != current_drive_id) {
                if (active_fs->init) active_fs->init();
                active_hardware_drive = current_drive_id;
            }
            break;
        }
    }
    
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
/*
void fs_list_files(const char* path) {
    // 1. Rădăcina globală VFS
    if (is_at_global_root && (!path || path[0] == '\0' || streq(path, "/") || streq(path, "."))) {
        print("VFS Root Directory Contents:\n");
        int count = 0;
        for (int i = 0; i < MAX_MOUNTS; i++) {
            if (mount_table[i].in_use) {
                char output[128];
                const char* fs_name = mount_table[i].fs ? mount_table[i].fs->name : "unknown";
                
                // FORMAT CORECTAT: %s simplu (fără cifre care să încurce snprintf-ul)
                snprintf(output, sizeof(output), "[DIR]  %s (FS: %s | Drive ID: %d)\n", 
                         mount_table[i].mount_point, fs_name, mount_table[i].drive_id);
                print(output);
                count++;
            }
        }
        if (count == 0) print(" (Niciun disc montat)\n");
        return;
    }

    // 2. Parsarea căii și găsirea driverului
    char local_path[128];
    FileSystemInterface* target = vfs_route(path, local_path);
    
    // Nu mai forțăm local_path să devină "/" aici!
    // Dacă e gol "", driverul (ex: fs_v3.c) știe să își folosească directorul curent din PCB.

    // 3. Trimitem comanda către driver
    if (target && target->list_files) {
        target->list_files(local_path);
    } else {
        print("Eroare: Driver invalid sau FS nu suporta listarea.\n");
    }
}
*/

void fs_list_files(const char* path) {
    // 1. Rădăcina globală VFS
    if (is_at_global_root && (!path || path[0] == '\0' || streq(path, "/") || streq(path, "."))) {
        print("VFS Root Directory Contents:\n");
        int count = 0;
        for (int i = 0; i < MAX_MOUNTS; i++) {
            if (mount_table[i].in_use) {
                char output[128];
                const char* fs_name = mount_table[i].fs ? mount_table[i].fs->name : "unknown";
                
                // Format stil UNIX: drwxr-xr-x nano nano <mount_point> (FS: ... | Drive: ...)
                snprintf(output, sizeof(output), "drwxr-xr-x  nano  nano  %s  (FS: %s | Drive ID: %d)\n", 
                         mount_table[i].mount_point, fs_name, mount_table[i].drive_id);
                print(output);
                count++;
            }
        }
        if (count == 0) print(" (Niciun disc montat)\n");
        return;
    }

    // 2. Parsarea căii și găsirea driverului
    char local_path[128];
    FileSystemInterface* target = vfs_route(path, local_path);

    // 3. Trimitem comanda către driver
    if (target && target->list_files) {
        target->list_files(local_path);
    } else {
        print("Eroare: Driver invalid sau FS nu suporta listarea.\n");
    }
}


// Extrage calea directorului părinte (ex: "/test/aa" -> "/test", "aa" -> "/")
static void vfs_get_parent_path(const char* full_path, char* parent_out) {
    int len = 0;
    int last_slash = -1;
    while (full_path[len] && len < 127) {
        if (full_path[len] == '/') last_slash = len;
        len++;
    }

    if (last_slash <= 0) {
        // Fișierul e în rădăcina partiției curente
        parent_out[0] = '/';
        parent_out[1] = '\0';
    } else {
        for (int i = 0; i < last_slash; i++) {
            parent_out[i] = full_path[i];
        }
        parent_out[last_slash] = '\0';
    }
}


// -------------------------------------------------------------------------
// CITIRE CU VERIFICARE PERMISIUNI (MAY_READ = 4)
// -------------------------------------------------------------------------
int fs_read_file(const char* name, uint8_t* buffer, uint32_t max_size) {
    char local_path[128];
    FileSystemInterface* target = vfs_route(name, local_path);

    if (!target) return 0;

    // Verificăm permisiunile prin VFS
    if (target->stat) {
        fs_stat_t st;
        if (target->stat(local_path, &st)) {
            extern PCB* current_process;
            if (!fs_check_access(&st, 4, current_process)) { // 4 = READ
                return 0; // Acces refuzat!
            }
        }
    }

    if (target->read_file) return target->read_file(local_path, buffer, max_size);
    return 0;
}
// -------------------------------------------------------------------------
// CREARE FIȘIER (Verifică MAY_WRITE pe folderul părinte și aplică UID/GID)
// -------------------------------------------------------------------------
int fs_create_file(const char* name, uint32_t size, uint16_t mode) { // <-- Am adăugat 'mode'
    if (!name || name[0] == '\0') return 0;
    
    // Presupunând că ai 'is_at_global_root' global
    extern int is_at_global_root; 
    if (is_at_global_root && name[0] != '/') return 0;

    char local_path[128];
    FileSystemInterface* target = vfs_route(name, local_path);
    if (!target) return 0;

    // Aducem referința la procesul curent aici, sus
    extern PCB* current_process;

    // 1. Verificăm permisiunile pe folderul unde vrem să creăm fișierul
    if (target->stat) {
        char parent_path[128];
        vfs_get_parent_path(local_path, parent_path);

        fs_stat_t st;
        if (target->stat(parent_path, &st)) {
            if (!fs_check_access(&st, 2, current_process)) { // 2 = MAY_WRITE
                return 0; // Acces refuzat pe directorul părinte!
            }
        }
    }

    // 2. Extragem proprietarul (UID) și grupul (GID) din procesul curent
    uint32_t current_uid = current_process ? current_process->uid : 0;
    uint32_t current_gid = current_process ? current_process->gid : 0;

    // 3. Mod implicit pentru fișiere dacă nu e specificat (ex: 0644 = rw-r--r--)
    if (mode == 0) {
        mode = 0644;
    }

    // 4. Creăm fișierul transmițând toate datele către driver (ex: NAN2)
    if (target->create_file) {
        return target->create_file(local_path, size, current_uid, current_gid, mode);
    }
    
    return 0;
}
// -------------------------------------------------------------------------
// SCRIERE CU VERIFICARE PERMISIUNI (MAY_WRITE = 2)
// -------------------------------------------------------------------------
int fs_write_file(const char* name, const uint8_t* data, uint32_t size) {
    char local_path[128];
    FileSystemInterface* target = vfs_route(name, local_path);

    if (!target) return 0;

    if (target->stat) {
        fs_stat_t st;
        if (target->stat(local_path, &st)) {
            extern PCB* current_process;
            if (!fs_check_access(&st, 2, current_process)) { // 2 = WRITE
                return 0; // Acces refuzat!
            }
        }
    }

    if (target->write_file) return target->write_file(local_path, data, size);
    return 0;
}

// -------------------------------------------------------------------------
// ȘTERGERE FIȘIER (Verifică MAY_WRITE pe folderul părinte)
// -------------------------------------------------------------------------
int fs_delete_file(const char* name) {
    char local_path[128];
    FileSystemInterface* target = vfs_route(name, local_path);
    if (!target) return 0;

    if (target->stat) {
        char parent_path[128];
        vfs_get_parent_path(local_path, parent_path);

        fs_stat_t st;
        if (target->stat(parent_path, &st)) {
            extern PCB* current_process;
            if (!fs_check_access(&st, 2, current_process)) { // 2 = MAY_WRITE
                return 0; // Acces refuzat pe directorul părinte!
            }
        }
    }

    if (target->delete_file) return target->delete_file(local_path);
    return 0;
}

// -------------------------------------------------------------------------
// CREARE DIRECTOR (Verifică MAY_WRITE pe folderul părinte și aplică UID/GID)
// -------------------------------------------------------------------------
int fs_mkdir(const char* name, uint16_t mode) { // <-- Am adăugat 'mode' (ex: 0755)
    char local_path[128];
    FileSystemInterface* target = vfs_route(name, local_path);
    if (!target) return 0;

    // Aducem referința la procesul curent mai sus, pentru a o folosi în ambele locuri
    extern PCB* current_process;

    // 1. Verificare permisiuni (MAY_WRITE) pe directorul părinte
    if (target->stat) {
        char parent_path[128];
        vfs_get_parent_path(local_path, parent_path);

        fs_stat_t st;
        if (target->stat(parent_path, &st)) {
            if (!fs_check_access(&st, 2, current_process)) { // 2 = MAY_WRITE
                return 0; // Acces refuzat pe directorul părinte!
            }
        }
    }

    // 2. Extragem UID și GID de la procesul care apelează funcția
    uint32_t current_uid = current_process ? current_process->uid : 0;
    uint32_t current_gid = current_process ? current_process->gid : 0;

    // Dacă nu e specificat un mod, punem unul implicit pentru directoare (rwxr-xr-x)
    if (mode == 0) {
        mode = 0755; 
    }

    // 3. Creăm efectiv directorul trimițând UID-ul și GID-ul driverului fizic (ex: NAN2)
    if (target->mkdir) {
        return target->mkdir(local_path, current_uid, current_gid, mode);
    }
    
    return 0;
}

// -------------------------------------------------------------------------
// ȘTERGERE DIRECTOR (Verifică MAY_WRITE pe folderul părinte)
// -------------------------------------------------------------------------
int fs_rmdir(const char* name) {
    char local_path[128];
    FileSystemInterface* target = vfs_route(name, local_path);
    if (!target) return 0;

    if (target->stat) {
        char parent_path[128];
        vfs_get_parent_path(local_path, parent_path);

        fs_stat_t st;
        if (target->stat(parent_path, &st)) {
            extern PCB* current_process;
            if (!fs_check_access(&st, 2, current_process)) { // 2 = MAY_WRITE
                return 0; // Acces refuzat pe directorul părinte!
            }
        }
    }

    if (target->rmdir) return target->rmdir(local_path);
    return 0;
}


int fs_cd(const char* name) {
    if (!name || name[0] == '\0') return 0;

    // 1. Comanda "cd /" sau "cd /." ne aduce înapoi în meniul global VFS
    if (streq(name, "/") || streq(name, "/.")) {
        is_at_global_root = 1;
        // Resetează starea procesului curent
        extern PCB* current_process;
        if (current_process) {
            current_process->cwd_sector = 0;
            current_process->current_path[0] = '/';
            current_process->current_path[1] = '\0';
        }
        return 1;
    }

    // 2. Dacă suntem în interiorul unei partiții și tastăm "cd .." care duce la rădăcina partiției
    if (!is_at_global_root && streq(name, "..")) {
        char current_sub_path[64];
        if (active_fs && active_fs->get_current_path) {
            active_fs->get_current_path(current_sub_path, 64);
            if (streq(current_sub_path, "/")) {
                is_at_global_root = 1;
                return 1;
            }
        } else {
            is_at_global_root = 1;
            return 1;
        }
    }

    // 3. Pregătim calea pentru rutare
    char full_target_path[128];
    if (is_at_global_root) {
        // Dacă suntem în root global și utilizatorul tastează un nume fără "/" (ex: "hda" în loc de "/hda")
        if (name[0] != '/') {
            full_target_path[0] = '/';
            string_copy(full_target_path + 1, name);
        } else {
            string_copy(full_target_path, name);
        }
    } else {
        // Dacă suntem deja în interiorul unei partiții, păstrăm calea primită
        string_copy(full_target_path, name);
    }

    // 4. Folosim vfs_route pentru a găsi driverul corect și calea locală pe disc
    char local_path[128];
    FileSystemInterface* target = vfs_route(full_target_path, local_path);

    if (target) {
        // Dacă calea locală e goală (ex: am tastat exact "/hda"), o forțăm la "/"
        if (local_path[0] == '\0') {
            string_copy(local_path, "/");
        }

        // Dacă driverul are funcție de CD, o apelăm
        if (target->cd) {
            int success = target->cd(local_path);
            if (success) {
                active_fs = target;
                is_at_global_root = 0; // Ieșim din global root, am intrat pe disc
                return 1;
            } else {
                return 0; // Directorul nu există pe disc
            }
        } else {
            // Fallback pentru drivere fără CD explicit
            active_fs = target;
            is_at_global_root = 0;
            return 1;
        }
    }

    return 0;
}

// VFS Get Current Path - Complet Dinamic
void fs_get_current_path(char* buffer, uint32_t max_len) {
    // Dacă suntem în meniul principal VFS
    if (is_at_global_root) {
        buffer[0] = '/';
        buffer[1] = '\0';
        return;
    }

    // Aflăm unde suntem în interiorul partiției curente
    char sub_path[64];
    sub_path[0] = '/';
    sub_path[1] = '\0';

    if (active_fs && active_fs->get_current_path) {
        active_fs->get_current_path(sub_path, 64);
    }

    // Găsim numele partiției (prefixul) căutând drive-ul activ în tabela de montare
    const char* prefix = "/unknown";
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (mount_table[i].in_use && mount_table[i].drive_id == current_drive_id) {
            prefix = mount_table[i].mount_point;
            break;
        }
    }
    
    // Construim calea finală combinând Prefixul (ex: /hda) cu SubCalea (ex: /sbin)
    int i = 0;
    while (prefix[i] && i < (int)max_len - 1) {
        buffer[i] = prefix[i];
        i++;
    }

    // Dacă sub-calea e doar "/", nu o mai adăugăm ca să nu iasă "/hda/"
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

int fs_append_file(const char* path, const char* data) {
    char local_path[128];
    FileSystemInterface* target = vfs_route(path, local_path);
    
    if (target && target->append_file) {
        target->append_file(local_path, data);
    }
	return 0; // Driver-ul nu suportă append sau calea nu e validă
}

int fs_check_access(fs_stat_t* stat, int mode, PCB* proc) {
    if (!stat || !proc) return 0;

    // Root (UID 0) trece de orice verificare
    if (proc->uid == 0) return 1;

    uint16_t perm = 0;
    if (proc->uid == stat->uid) {
        perm = (stat->mode >> 6) & 0x7; // Drepturile proprietarului (User)
    } else if (proc->gid == stat->gid) {
        perm = (stat->mode >> 3) & 0x7; // Drepturile grupului (Group)
    } else {
        perm = stat->mode & 0x7;        // Drepturile altora (Others)
    }

    return (perm & mode) == mode;
}

int fs_chmod(const char* path, uint16_t mode) {
    char local_path[128];
    FileSystemInterface* target = vfs_route(path, local_path);
    if (!target || !target->chmod) return 0;

    // TODO (Securitate): Aici poți chema target->stat() pentru a verifica 
    // dacă current_process->uid == 0 sau current_process->uid == stat.uid
    
    return target->chmod(local_path, mode);
}

int fs_chown(const char* path, uint32_t uid, uint32_t gid) {
    char local_path[128];
    FileSystemInterface* target = vfs_route(path, local_path);
    if (!target || !target->chown) return 0;
    
    // Doar root (UID 0) ar trebui să poată schimba proprietarul oricărui fișier
    extern PCB* current_process;
    if (current_process && current_process->uid != 0) {
        return 0; // Operation not permitted
    }

    return target->chown(local_path, uid, gid);
}