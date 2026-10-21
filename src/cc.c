#include "nano_libc.h"
#include "nano_string.h"

// Funcție pentru a căuta un șir în alt șir (simplă, fără dependențe externe)
char* my_strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char*)haystack;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        nano_print("Utilizare: cc <sursa.c> <iesire.s>\n");
        return 1;
    }

    const char* src_file = argv[1];
    const char* out_file = argv[2];

    nano_print("[cc] Compilator Nano C (main, print, exit) pornit...\n");

    // 1. Citim fișierul C în memorie
    uint8_t* src_buffer = (uint8_t*)nano_malloc(8192);
    if (!src_buffer) {
        nano_print("[cc] Eroare: Memorie insuficienta!\n");
        return 1;
    }

    int bytes_read = nano_read_file(src_file, src_buffer, 8191);
    if (bytes_read <= 0) {
        nano_print("[cc] Eroare: Nu s-a putut citi fisierul sursa C.\n");
        nano_free(src_buffer);
        return 1;
    }
    src_buffer[bytes_read] = '\0';

    // 2. Creăm fișierul de asamblor de ieșire (.s)
    nano_create_file(out_file, 0);

    // Buffer în care vom construi tot conținutul fișierului .s
    char asm_out[4096];
    int asm_idx = 0;

    // Adăugăm eticheta de start pentru NanoASM
    const char* header = "; Generat automat de cc.c (Nano C Subset)\n\nmain:\n";
    for (int i = 0; header[i] != '\0'; i++) asm_out[asm_idx++] = header[i];

    // 3. Parsăm simplu linie cu linie sau căutăm apelurile de print
    // Căutăm cuvântul "print" în fișierul sursă C
    char* ptr = (char*)src_buffer;
    while (*ptr != '\0') {
        // Căutăm apariția lui "print"
        char* print_pos = my_strstr(ptr, "print");
        if (!print_pos) break;

        // Căutăm ghilimelele deschise după print
        char* quote_start = my_strstr(print_pos, "\"");
        if (!quote_start) break;

        quote_start++; // Trecem peste prima ghilimea
        char* quote_end = my_strstr(quote_start, "\"");
        if (!quote_end) break;

        // Extragem conținutul dintre ghilimele
        char message[256];
        int m_len = 0;
        char* q = quote_start;
        while (q < quote_end && m_len < 255) {
            message[m_len++] = *q++;
        }
        message[m_len] = '\0';

        // Generăm linia de NanoASM: print "mesaj"
        // Construim: print "
        const char* p_cmd = "    print \"";
        for (int i = 0; p_cmd[i] != '\0'; i++) asm_out[asm_idx++] = p_cmd[i];
        
        // Adăugăm mesajul
        for (int i = 0; message[i] != '\0'; i++) asm_out[asm_idx++] = message[i];
        
        // Închidem: "\n    newline\n"
        const char* p_end = "\"\n    newline\n";
        for (int i = 0; p_end[i] != '\0'; i++) asm_out[asm_idx++] = p_end[i];

        // Avansăm pointerul pentru a continua căutarea
        ptr = quote_end + 1;
    }

    // Adăugăm instrucțiunea de exit la sfârșit
    const char* footer = "    exit\n";
    for (int i = 0; footer[i] != '\0'; i++) asm_out[asm_idx++] = footer[i];
    asm_out[asm_idx] = '\0';

    // 4. Scriem tot rezultatul în fișierul `.s`
    nano_write_file(out_file, (const uint8_t*)asm_out, asm_idx);

    nano_print("[cc] Compilare din C în ASM finalizată cu succes!\n");

    nano_free(src_buffer);
    return 0;
}