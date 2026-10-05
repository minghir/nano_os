#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Utilizare: %s <fisier.psf> <output.h>\n", argv[0]);
        return 1;
    }

    FILE* f = fopen(argv[1], "rb");
    if (!f) {
        printf("Eroare: Nu pot deschide fișierul PSF sursă!\n");
        return 1;
    }

    uint8_t magic[2];
    fread(magic, 1, 2, f);

    int num_glyphs = 256;
    int char_size = 16;
    long header_size = 4;

    // Verificăm formatul PSF1 (Magic: 0x36 0x04)
    if (magic[0] == 0x36 && magic[1] == 0x04) {
        uint8_t mode, size;
        fread(&mode, 1, 1, f);
        fread(&size, 1, 1, f);
        char_size = size;
        num_glyphs = (mode & 0x01) ? 512 : 256;
        header_size = 4;
    } else {
        // Verificăm formatul PSF2 (Magic: 0x72 0xb5 0x4a 0x86)
        fseek(f, 0, SEEK_SET);
        uint32_t magic2;
        fread(&magic2, 4, 1, f);
        
        if (magic2 == 0x864ab572) { // Reprezentarea little-endian a antetului PSF2
            uint32_t h_size, n_glyphs, b_per_glyph;
            
            fseek(f, 8, SEEK_SET);
            fread(&h_size, 4, 1, f);      // Mărimea antetului
            fseek(f, 16, SEEK_SET);
            fread(&n_glyphs, 4, 1, f);    // Numărul de glife (caractere)
            fread(&b_per_glyph, 4, 1, f); // Bytes per glifă (înălțimea)
            
            header_size = h_size;
            num_glyphs = n_glyphs;
            char_size = b_per_glyph;
        } else {
            printf("Eroare: Fișierul nu este un format valid PSF1 sau PSF2!\n");
            fclose(f);
            return 1;
        }
    }

    // Trecem peste antet și citim datele brute ale fontului
    fseek(f, header_size, SEEK_SET);
    uint8_t* font_data = malloc(num_glyphs * char_size);
    if (!font_data) {
        printf("Eroare: Memorie insuficientă!\n");
        fclose(f);
        return 1;
    }
    
    fread(font_data, 1, num_glyphs * char_size, f);
    fclose(f);

    // Generăm fișierul header C de ieșire
    FILE* out = fopen(argv[2], "w");
    if (!out) {
        printf("Eroare: Nu pot crea fișierul C de ieșire!\n");
        free(font_data);
        return 1;
    }

    fprintf(out, "/* Generat automat din fișier PSF printr-un utilitar C */\n");
    fprintf(out, "#ifndef GENERATED_FONT_H\n#define GENERATED_FONT_H\n\n");
    fprintf(out, "static const unsigned char font_psf[%d][%d] = {\n", num_glyphs, char_size);

    for (int i = 0; i < num_glyphs; i++) {
        fprintf(out, "    {");
        for (int j = 0; j < char_size; j++) {
            fprintf(out, "0x%02X%s", font_data[i * char_size + j], (j == char_size - 1) ? "" : ", ");
        }
        fprintf(out, "}, // 0x%02X\n", i);
    }

    fprintf(out, "};\n\n#endif // GENERATED_FONT_H\n");
    fclose(out);
    free(font_data);

    printf("Succes! S-a generat: %s (%d caractere, %d bytes/char)\n", argv[2], num_glyphs, char_size);
    return 0;
}