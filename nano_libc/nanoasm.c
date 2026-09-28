#include "nano_libc.h"

// Funcție care calculează exact dimensiunea în octeți a unei instrucțiuni asamblate
static int get_instruction_size(const char* line) {
    if (line[0] == '\0' || line[0] == ';') return 0;
    
    // Dacă e etichetă (se termină cu ':'), nu ocupă spațiu în binar
    int len = strlen(line);
    if (len > 0 && line[len - 1] == ':') return 0;

    if (strcmp(line, "ret") == 0) return 1;
    if (strcmp(line, "int 0x80") == 0) return 2;
    if (starts_with(line, "mov rax, ") || starts_with(line, "mov rdi, ") || 
        starts_with(line, "mov rsi, ") || starts_with(line, "mov rdx, ")) return 7;
    if (starts_with(line, "add rax, ") || starts_with(line, "sub rax, ") || starts_with(line, "cmp rax, ")) return 6;
    if (starts_with(line, "imul rax, ")) return 7;
    if (strcmp(line, "print_rax") == 0) return 15;
    if (starts_with(line, "jmp ")) return 5;
    if (starts_with(line, "je ")) return 6;
    
    if (starts_with(line, "print \"")) {
        // print generheaza: mov rax (7) + lea rdi (7) + int 0x80 (2) + jmp (2) + string + ret (1)
        int m_idx = 0;
        char* p = line + 7;
        while (*p != '"' && *p != '\0') { m_idx++; p++; }
        return 7 + 7 + 2 + 2 + (m_idx + 1) + 1;
    }
    
    return 0;
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    nano_print("NanoASM v1.0 - Asamblor Text -> Binar (cu salturi)\n");

    const char* source_filename = "program.s";
    const char* output_filename = "out";

    // 1. Citim fișierul sursă text de pe disc
    uint8_t src_buffer[512];
    int bytes_read = nano_read_file(source_filename, src_buffer, 511);

    if (bytes_read <= 0) {
        nano_print("Eroare: Nu s-a putut citi fisierul sursa 'program.s'!\n");
        return 0;
    }
    src_buffer[bytes_read] = '\0';

    // Structură pentru etichete (maxim 16 etichete)
    typedef struct {
        char name[32];
        int offset;
    } Label;
    Label labels[16];
    int label_count = 0;

    // --- PASUL 1: Scanăm etichetele și calculăm offset-urile exacte ---
    {
        int p_bin_idx = 8; // Trecem de Header-ul NAS1 (8 octeți)
        int p_line_start = 0;
        for (int i = 0; i <= bytes_read; i++) {
            if (src_buffer[i] == '\n' || src_buffer[i] == '\0') {
                char line[64];
                int l_len = i - p_line_start;
                if (l_len >= 63) l_len = 63;
                
                int l_idx = 0;
                for (int k = 0; k < l_len; k++) {
                    char c = src_buffer[p_line_start + k];
                    if (c == ';') break; // Comentariu, ignorăm restul liniei
                    if (l_idx == 0 && (c == ' ' || c == '\t' || c == '\r')) continue;
                    line[l_idx++] = c;
                }
                line[l_idx] = '\0';

                while (l_idx > 0 && (line[l_idx - 1] == '\r' || line[l_idx - 1] == ' ' || line[l_idx - 1] == '\t')) {
                    l_idx--;
                    line[l_idx] = '\0';
                }

                p_line_start = i + 1;

                if (line[0] == '\0' || line[0] == ';') continue;

                // Verificăm dacă este etichetă (ex: "start:")
                int len = strlen(line);
                if (len > 0 && line[len - 1] == ':') {
                    line[len - 1] = '\0';
                    strcpy(labels[label_count].name, line);
                    labels[label_count].offset = p_bin_idx;
                    label_count++;
                    continue;
                }

                p_bin_idx += get_instruction_size(line);
            }
        }
    }

    nano_print("Sursa citita cu succes. Se asambleaza...\n");

    // --- PASUL 2: Generarea efectivă a binarului ---
    uint8_t bin_buffer[512];
    int bin_idx = 8;

    NanoHeader* hdr = (NanoHeader*)bin_buffer;
    hdr->magic[0] = 'N'; hdr->magic[1] = 'A'; hdr->magic[2] = 'S'; hdr->magic[3] = '1';
    hdr->entry_offset = 8;

    int line_start = 0;
    for (int i = 0; i <= bytes_read; i++) {
        if (src_buffer[i] == '\n' || src_buffer[i] == '\0') {
            char line[64];
            int line_len = i - line_start;
            if (line_len >= 63) line_len = 63;
            
            int l_idx = 0;
            for (int k = 0; k < line_len; k++) {
                char c = src_buffer[line_start + k];
                if (c == ';') break; // Comentariu, ignorăm restul liniei
                if (l_idx == 0 && (c == ' ' || c == '\t' || c == '\r')) continue;
                line[l_idx++] = c;
            }
            line[l_idx] = '\0';

            while (l_idx > 0 && (line[l_idx - 1] == '\r' || line[l_idx - 1] == ' ' || line[l_idx - 1] == '\t')) {
                l_idx--;
                line[l_idx] = '\0';
            }

            line_start = i + 1;

            if (line[0] == '\0' || line[0] == ';') continue;
            
            int len = strlen(line);
            if (len > 0 && line[len - 1] == ':') continue; // Ignorăm etichetele în pasul 2

            // --- TRADUCEREA MNEMONICELOR ---
            
            if (strcmp(line, "ret") == 0) {
                bin_buffer[bin_idx++] = 0xC3;
            }
            else if (strcmp(line, "int 0x80") == 0) {
                bin_buffer[bin_idx++] = 0xCD;
                bin_buffer[bin_idx++] = 0x80;
            }
            else if (starts_with(line, "mov rax, ")) {
                int val = atoi(line + 9);
                bin_buffer[bin_idx++] = 0x48; bin_buffer[bin_idx++] = 0xc7; bin_buffer[bin_idx++] = 0xc0;
                bin_buffer[bin_idx++] = (uint8_t)(val & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 8) & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 16) & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 24) & 0xFF);
            }
            else if (starts_with(line, "mov rdi, ")) {
                int val = atoi(line + 9);
                bin_buffer[bin_idx++] = 0x48; bin_buffer[bin_idx++] = 0xc7; bin_buffer[bin_idx++] = 0xc7;
                bin_buffer[bin_idx++] = (uint8_t)(val & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 8) & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 16) & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 24) & 0xFF);
            }
            else if (starts_with(line, "mov rsi, ")) {
                int val = atoi(line + 9);
                bin_buffer[bin_idx++] = 0x48; bin_buffer[bin_idx++] = 0xc7; bin_buffer[bin_idx++] = 0xc6;
                bin_buffer[bin_idx++] = (uint8_t)(val & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 8) & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 16) & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 24) & 0xFF);
            }
            else if (starts_with(line, "mov rdx, ")) {
                int val = atoi(line + 9);
                bin_buffer[bin_idx++] = 0x48; bin_buffer[bin_idx++] = 0xc7; bin_buffer[bin_idx++] = 0xc2;
                bin_buffer[bin_idx++] = (uint8_t)(val & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 8) & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 16) & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 24) & 0xFF);
            }
            else if (starts_with(line, "print \"")) {
                char msg[128];
                int m_idx = 0;
                char* p = line + 7;
                while (*p != '"' && *p != '\0' && m_idx < 127) {
                    msg[m_idx++] = *p++;
                }
                msg[m_idx] = '\0';

                // mov rax, 1
                bin_buffer[bin_idx++] = 0x48; bin_buffer[bin_idx++] = 0xC7; bin_buffer[bin_idx++] = 0xC0;
                bin_buffer[bin_idx++] = 0x01; bin_buffer[bin_idx++] = 0x00; bin_buffer[bin_idx++] = 0x00; bin_buffer[bin_idx++] = 0x00;

                // lea rdi, [rip + offset]
                bin_buffer[bin_idx++] = 0x48; bin_buffer[bin_idx++] = 0x8D; bin_buffer[bin_idx++] = 0x3D;
                int disp_pos = bin_idx;
                bin_idx += 4;

                // int 0x80
                bin_buffer[bin_idx++] = 0xCD; bin_buffer[bin_idx++] = 0x80;

                // jmp peste text
                bin_buffer[bin_idx++] = 0xEB;
                int jmp_rel_pos = bin_idx++;

                int string_offset = bin_idx;
                for (int m = 0; m <= m_idx; m++) {
                    bin_buffer[bin_idx++] = (uint8_t)msg[m];
                }

                int32_t disp32 = string_offset - (disp_pos + 4);
                bin_buffer[disp_pos + 0] = (uint8_t)(disp32 & 0xFF);
                bin_buffer[disp_pos + 1] = (uint8_t)((disp32 >> 8) & 0xFF);
                bin_buffer[disp_pos + 2] = (uint8_t)((disp32 >> 16) & 0xFF);
                bin_buffer[disp_pos + 3] = (uint8_t)((disp32 >> 24) & 0xFF);

                int jmp_target = bin_idx;
                int jmp_from = jmp_rel_pos + 1;
                bin_buffer[jmp_rel_pos] = (uint8_t)(jmp_target - jmp_from);

                bin_buffer[bin_idx++] = 0xC3; // ret
            }
            else if (starts_with(line, "add rax, ")) {
                int val = atoi(line + 9);
                bin_buffer[bin_idx++] = 0x48; bin_buffer[bin_idx++] = 0x05;
                bin_buffer[bin_idx++] = (uint8_t)(val & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 8) & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 16) & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 24) & 0xFF);
            }
            else if (starts_with(line, "sub rax, ")) {
                int val = atoi(line + 9);
                bin_buffer[bin_idx++] = 0x48; bin_buffer[bin_idx++] = 0x2D;
                bin_buffer[bin_idx++] = (uint8_t)(val & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 8) & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 16) & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 24) & 0xFF);
            }
            else if (starts_with(line, "cmp rax, ")) {
                int val = atoi(line + 9);
                // Opcod pentru cmp rax, imm32: 48 3D [4 octeți little-endian]
                bin_buffer[bin_idx++] = 0x48;
                bin_buffer[bin_idx++] = 0x3D;
                bin_buffer[bin_idx++] = (uint8_t)(val & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 8) & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 16) & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 24) & 0xFF);
                
                nano_print("  [ASM] cmp rax, val -> opcod generat\n");
            }
            else if (starts_with(line, "imul rax, ")) {
                int val = atoi(line + 10);
                bin_buffer[bin_idx++] = 0x48; bin_buffer[bin_idx++] = 0x69; bin_buffer[bin_idx++] = 0xC0;
                bin_buffer[bin_idx++] = (uint8_t)(val & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 8) & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 16) & 0xFF);
                bin_buffer[bin_idx++] = (uint8_t)((val >> 24) & 0xFF);
            }
            else if (strcmp(line, "print_rax") == 0) {
                
                    // 1. Salvăm rax în rdi: mov rdi, rax (48 89 C7)
                    bin_buffer[bin_idx++] = 0x48; bin_buffer[bin_idx++] = 0x89; bin_buffer[bin_idx++] = 0xC7;

                    // 2. Setăm rax = 16 (SYSCALL_PRINT_INT): mov rax, 16 (48 C7 C0 10 00 00 00)
                    bin_buffer[bin_idx++] = 0x48; bin_buffer[bin_idx++] = 0xC7; bin_buffer[bin_idx++] = 0xC0;
                    bin_buffer[bin_idx++] = 0x10; bin_buffer[bin_idx++] = 0x00; bin_buffer[bin_idx++] = 0x00; bin_buffer[bin_idx++] = 0x00;

                    // 3. Apelăm întreruperea: int 0x80 (CD 80)
                    bin_buffer[bin_idx++] = 0xCD; bin_buffer[bin_idx++] = 0x80;

                    // 4. Restaurăm rax din rdi: mov rax, rdi (48 89 F8)
                    bin_buffer[bin_idx++] = 0x48; bin_buffer[bin_idx++] = 0x89; bin_buffer[bin_idx++] = 0xF8;

                    nano_print("  [ASM] print_rax (cu restaurare rax) -> opcod generat\n");
                
            }
           else if (starts_with(line, "jmp ") || starts_with(line, "je ")) {
                int is_je = starts_with(line, "je ");
                char* target_label = line + (is_je ? 3 : 4);

                // Curățăm spațiile de la început
                while (*target_label == ' ' || *target_label == '\t') {
                    target_label++;
                }

                // Curățăm spațiile sau \r de la sfârșitul etichetei țintă
                int t_len = strlen(target_label);
                while (t_len > 0 && (target_label[t_len - 1] == '\r' || target_label[t_len - 1] == ' ' || target_label[t_len - 1] == '\t')) {
                    target_label[t_len - 1] = '\0';
                    t_len--;
                }

                // Căutăm etichetă în tabel
                int target_offset = -1;
                for (int l = 0; l < label_count; l++) {
                    if (strcmp(labels[l].name, target_label) == 0) {
                        target_offset = labels[l].offset;
                        break;
                    }
                }

                if (target_offset == -1) {
                    nano_print("  [EROARE] Eticheta negasita: '");
                    nano_print(target_label);
                    nano_print("'\n");
                }

                // Opcoduri: JMP rel32 = E9 [rel32], JE rel32 = 0F 84 [rel32]
                if (is_je) {
                    bin_buffer[bin_idx++] = 0x0F;
                    bin_buffer[bin_idx++] = 0x84;
                } else {
                    bin_buffer[bin_idx++] = 0xE9;
                }

                int rel_pos = bin_idx;
                bin_idx += 4;

                int32_t rel32 = target_offset - (rel_pos + 4);
                bin_buffer[rel_pos + 0] = (uint8_t)(rel32 & 0xFF);
                bin_buffer[rel_pos + 1] = (uint8_t)((rel32 >> 8) & 0xFF);
                bin_buffer[rel_pos + 2] = (uint8_t)((rel32 >> 16) & 0xFF);
                bin_buffer[rel_pos + 3] = (uint8_t)((rel32 >> 24) & 0xFF); // Notă: corectat la rel_pos + 3
            }
            else {
                nano_print("  [EROARE] Instructiune necunoscuta: ");
                nano_print(line);
                nano_print("\n");
            }
        }
    }

    // 4. Salvăm binarul generat pe disc
    nano_print("Se salveaza binarul 'out'...\n");
    if (nano_create_file(output_filename, bin_idx)) {
        if (nano_write_file(output_filename, bin_buffer, bin_idx)) {
            nano_print("Asamblare completă cu salturi finalizată!\n");
            nano_print("Poti rula acum 'out'\n");
        } else {
            nano_print("Eroare la scrierea binarului!\n");
        }
    } else {
        nano_print("Eroare la crearea fisierului binar!\n");
    }

    return 0;
}
