#include "nano_libc.h"
#include "nano_string.h"

// Constante ELF minime
#define EI_NIDENT 16
#define PT_LOAD 1

// Structura Header-ului principal ELF64
typedef struct {
    uint8_t  e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

// Structura Program Header ELF64
typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

const uint8_t nas1_magic[8] = {'N', 'A', 'S', '1', 0x08, 0x00, 0x00, 0x00};
const uint8_t elf_magic[4]  = {0x7f, 'E', 'L', 'F'};

int main(int argc, char **argv) {
    if (argc != 3) {
        nano_print("Utilizare: elf2nas <input.elf> <output.bin>\n");
        return 1;
    }

    // 1. Citim tot fișierul ELF în memorie folosind funcția din nano_libc.h
    uint8_t* elf_data = 0;
    uint32_t elf_size = read_file_to_buffer(argv[1], &elf_data);
    
    if (elf_size == 0 || !elf_data) {
        nano_print("Eroare: Nu s-a putut citi fisierul de intrare.\n");
        return 1;
    }

    if (elf_size < sizeof(Elf64_Ehdr)) {
        nano_print("Eroare: Fisierul este prea mic pentru a fi un ELF.\n");
        nano_free(elf_data);
        return 1;
    }

    // 2. Validăm header-ul ELF direct din memoria alocată
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)elf_data;
    if (ehdr->e_ident[0] != elf_magic[0] || ehdr->e_ident[1] != elf_magic[1] || 
        ehdr->e_ident[2] != elf_magic[2] || ehdr->e_ident[3] != elf_magic[3]) {
        nano_print("Eroare: Fisierul nu este un ELF valid.\n");
        nano_free(elf_data);
        return 1;
    }

    // 3. Calculăm dimensiunea noului binar găsind capetele segmentelor
    uint64_t base_addr = (uint64_t)-1;
    uint64_t max_addr = 0;

    for (int i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr* phdr = (Elf64_Phdr*)(elf_data + ehdr->e_phoff + i * sizeof(Elf64_Phdr));
        if (phdr->p_type == PT_LOAD && phdr->p_filesz > 0) {
            if (phdr->p_vaddr < base_addr) {
                base_addr = phdr->p_vaddr;
            }
            if (phdr->p_vaddr + phdr->p_filesz > max_addr) {
                max_addr = phdr->p_vaddr + phdr->p_filesz;
            }
        }
    }

    if (base_addr == (uint64_t)-1) {
        nano_print("Eroare: Nu s-au gasit segmente de cod incarcabile.\n");
        nano_free(elf_data);
        return 1;
    }

    // 4. Alocăm memorie pentru binarul de ieșire (dimensiune cod + 8 bytes header)
    uint32_t output_size = 8 + (max_addr - base_addr);
    uint8_t* output_data = (uint8_t*)nano_malloc(output_size);
    
    if (!output_data) {
        nano_print("Eroare: Memorie insuficienta pentru conversie.\n");
        nano_free(elf_data);
        return 1;
    }

    // Curățăm memoria cu zero (important pentru padding între .text și .data)
    for (uint32_t i = 0; i < output_size; i++) {
        output_data[i] = 0;
    }

    // 5. Punem header-ul NAS1 la începutul binarului
    for (int i = 0; i < 8; i++) {
        output_data[i] = nas1_magic[i];
    }

    // 6. Extragem segmentele și le mutăm la locul lor
    for (int i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr* phdr = (Elf64_Phdr*)(elf_data + ehdr->e_phoff + i * sizeof(Elf64_Phdr));
        if (phdr->p_type == PT_LOAD && phdr->p_filesz > 0) {
            
            uint32_t dest_offset = 8 + (phdr->p_vaddr - base_addr);
            uint8_t* src_ptr = elf_data + phdr->p_offset;
            
            for (uint64_t j = 0; j < phdr->p_filesz; j++) {
                output_data[dest_offset + j] = src_ptr[j];
            }
        }
    }

    // 7. Salvăm rezultatul plat pe disc folosind syscall-urile Nano OS
    nano_create_file(argv[2], output_size);
    nano_write_file(argv[2], output_data, output_size);

    nano_print("[elf2nas] Binarul NAS1 a fost generat cu succes!\n");

    // Curățenie
    nano_free(elf_data);
    nano_free(output_data);

    return 0;
}