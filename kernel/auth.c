#include "auth.h"
#include "pcb.h"
#include "memory.h"
#include "syslog.h"
#include "fs/fs.h"
#include "string.h"

extern PCB* current_process;

uint32_t get_uid(void) {
    if (current_process) {
        return current_process->uid;
    }
    return 0; // Fallback pe root/nano
}

UserAccount user_database[MAX_USERS];
int user_count = 0;

void auth_init(void) {
    uint8_t* buffer = (uint8_t*)malloc(1024);
    if (!buffer) {
        KLOG_ERROR("AUTH: Memory allocation failed for usr.cfg buffer!\n");
        return;
    }

    for (int i = 0; i < 1024; i++) buffer[i] = 0;
    user_count = 0;

    int bytes = fs_read_file("/hda/cfg/usr.cfg", buffer, 1023);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        char* text = (char*)buffer;

        int pos = 0;
        while (pos < bytes && user_count < MAX_USERS) {
            while (pos < bytes && (text[pos] == '\n' || text[pos] == '\r')) {
                pos++;
            }
            if (pos >= bytes) break;

            char line[128];
            int l_idx = 0;
            while (pos < bytes && text[pos] != '\n' && text[pos] != '\r' && l_idx < 127) {
                if (text[pos] == '#') {
                    while (pos < bytes && text[pos] != '\n' && text[pos] != '\r') {
                        pos++;
                    }
                    break;
                }
                line[l_idx++] = text[pos++];
            }
            line[l_idx] = '\0';

            if (l_idx == 0) continue;

            char username[32] = {0};
            char uid_str[16]  = {0};
            char gid_str[16]  = {0};
            char home[64]     = {0};

            int field = 0, field_idx = 0;
            for (int i = 0; line[i] != '\0'; i++) {
                if (line[i] == ':') {
                    field++;
                    field_idx = 0;
                    continue;
                }

                if (field == 0 && field_idx < 31) username[field_idx++] = line[i];
                else if (field == 1 && field_idx < 15) uid_str[field_idx++] = line[i];
                else if (field == 2 && field_idx < 15) gid_str[field_idx++] = line[i];
                else if (field == 3 && field_idx < 63) home[field_idx++] = line[i];
            }

            if (field >= 1 && username[0] != '\0') {
                int k = 0;
                while (username[k] != '\0' && k < MAX_USERNAME_LEN - 1) {
                    user_database[user_count].username[k] = username[k];
                    k++;
                }
                user_database[user_count].username[k] = '\0';

                uint32_t uid_val = 0;
                for (int u = 0; uid_str[u] >= '0' && uid_str[u] <= '9'; u++) {
                    uid_val = uid_val * 10 + (uid_str[u] - '0');
                }
                user_database[user_count].uid = uid_val;

                uint32_t gid_val = 0;
                for (int g = 0; gid_str[g] >= '0' && gid_str[g] <= '9'; g++) {
                    gid_val = gid_val * 10 + (gid_str[g] - '0');
                }
                user_database[user_count].gid = gid_val;

                k = 0;
                while (home[k] != '\0' && k < 63) {
                    user_database[user_count].home_dir[k] = home[k];
                    k++;
                }
                user_database[user_count].home_dir[k] = '\0';

                user_count++;
            }
        }

        KLOG_INFO("AUTH: Utilizatori incarcati din /hda/cfg/usr.cfg cu succes.\n");
    } else {
        KLOG_WARNING("AUTH: /hda/cfg/usr.cfg missing! Fallback pe utilizatori default.\n");
        user_database[0] = (UserAccount){ "nano", 0, 0, "/hda" };
        user_database[1] = (UserAccount){ "user", 1000, 1000, "/hda/home/user" };
        user_count = 2;
    }

    free(buffer);
}

// Helper intern pentru verificarea apartenenței la un grup dintr-o listă
static int string_contains_token(const char* list, const char* token, char delim) {
    if (!list || !token) return 0;

    char buf[64];
    int b_idx = 0;
    int i = 0;

    while (1) {
        if (list[i] == delim || list[i] == '\0' || list[i] == '\n' || list[i] == '\r') {
            buf[b_idx] = '\0';

            char* trimmed = buf;
            while (*trimmed == ' ') trimmed++;

            if (trimmed[0] != '\0' && string_compare(trimmed, token) == 0) {
                return 1;
            }

            b_idx = 0;
            if (list[i] == '\0') break;
        } else if (b_idx < 63) {
            buf[b_idx++] = list[i];
        }
        i++;
    }
    return 0;
}

void load_user_groups(const char* username, PCB* proc) {
    if (!proc) return;
    proc->group_count = 0;

    uint8_t buffer[1024];
    int bytes = fs_read_file("/hda/cfg/grp.cfg", buffer, 1023);
    if (bytes <= 0) return;

    buffer[bytes] = '\0';
    char* text = (char*)buffer;
    int pos = 0;

    while (pos < bytes && proc->group_count < MAX_GROUPS) {
        while (pos < bytes && (text[pos] == '\n' || text[pos] == '\r')) pos++;
        if (pos >= bytes) break;
        if (text[pos] == '#') {
            while (pos < bytes && text[pos] != '\n' && text[pos] != '\r') pos++;
            continue;
        }

        char line[128];
        int l_idx = 0;
        while (pos < bytes && text[pos] != '\n' && text[pos] != '\r' && l_idx < 127) {
            line[l_idx++] = text[pos++];
        }
        line[l_idx] = '\0';

        char gid_str[16] = {0};
        char members[64] = {0};
        int field = 0, f_idx = 0;

        for (int i = 0; line[i] != '\0'; i++) {
            if (line[i] == ':') { field++; f_idx = 0; continue; }
            if (field == 1 && f_idx < 15) gid_str[f_idx++] = line[i];
            else if (field == 2 && f_idx < 63) members[f_idx++] = line[i];
        }

        if (field >= 2 && members[0] != '\0') {
            if (string_contains_token(members, username, ',')) {
                uint32_t gid_val = 0;
                for (int g = 0; gid_str[g] >= '0' && gid_str[g] <= '9'; g++) {
                    gid_val = gid_val * 10 + (gid_str[g] - '0');
                }

                if (gid_val != proc->gid) {
                    proc->groups[proc->group_count++] = gid_val;
                }
            }
        }
    }
}