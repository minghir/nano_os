/*
 *  TCC - Tiny C Compiler (Adaptat pentru Nano OS)
 *  
 *  Copyright (c) 2001-2004 Fabrice Bellard
 */

#ifndef ONE_SOURCE
# define ONE_SOURCE 1
#endif

// Include-ul sistemului tău de operare
#include "nano_libc.h"

// Eliminăm dependențele din C standard prin redifinitii
#include "nano_tcc_compat.h"

#include "tcc.h"
#if ONE_SOURCE
# include "libtcc.c"
#endif
#include "tcctools.c"

static const char help[] =
    "Tiny C Compiler "TCC_VERSION" - Nano OS Native Edition\n"
    "Usage: tcc [options...] [-o outfile] [-c] infile(s)...\n"
    "General options:\n"
    "  -c            compile only - generate an object file\n"
    "  -o outfile    set output filename\n"
    "  -Dsym[=val]   define 'sym' with value 'val'\n"
    "  -Idir         add include path 'dir'\n"
    "  -v --version  show version\n"
    "  -h            show help\n";

static const char version[] =
    "tcc version "TCC_VERSION" (x86_64 Nano OS Native)\n";

static void print_dirs(const char *msg, char **paths, int nb_paths)
{
    int i;
    nano_printf("%s:\n%s", msg, nb_paths ? "" : "  -\n");
    for(i = 0; i < nb_paths; i++)
        nano_printf("  %s\n", paths[i]);
}

static void print_search_dirs(TCCState *s)
{
    nano_printf("install: %s\n", s->tcc_lib_path);
    print_dirs("include", s->sysinclude_paths, s->nb_sysinclude_paths);
    print_dirs("libraries", s->library_paths, s->nb_library_paths);
}

// În Nano OS nu avem variabile de mediu (getenv), așa că deactivăm căutarea
static void set_environment(TCCState *s)
{
    /* În Nano OS căile sunt fixe (ex: /include, /lib) */
    tcc_add_sysinclude_path(s, "/include");
    tcc_add_library_path(s, "/lib");
}

static char *default_outputfile(TCCState *s, const char *first_file)
{
    char buf[1024];
    char *ext;
    const char *name = "a";

    if (first_file && strcmp(first_file, "-"))
        name = tcc_basename(first_file);
    if (strlen(name) + 4 >= sizeof buf)
        name = "a";
    strcpy(buf, name);
    ext = tcc_fileextension(buf);

    if ((s->just_deps || s->output_type == TCC_OUTPUT_OBJ) && !s->option_r && *ext)
        strcpy(ext, ".o");
    else
        strcpy(buf, "a.bin"); // În Nano OS output-ul implicit este .bin
    return tcc_strdup(buf);
}

// Timer simplificat pentru Nano OS
static unsigned getclock_ms(void)
{
    return 0; // Sau poți folosi un counter/ticks din kernelul Nano OS
}

// Wrapper peste salvarea fișierului de ieșire pentru a injecta Antetul NAS1
static int nano_output_file(TCCState *s, const char *filename) {
    // Dacă generăm un executabil binar
    if (s->output_type == TCC_OUTPUT_EXE || s->output_type == TCC_OUTPUT_BINARY) {
        
        // Deschidem fișierul de ieșire prin driverul tău EXT2
        FILE *f = fopen(filename, "wb");
        if (!f) {
            nano_printf("Eroare: Nu s-a putut crea fisierul %s\n", filename);
            return -1;
        }

        // 1. Construim Header-ul NAS1 (8 octeți)
        uint8_t nas1_header[8];
        nas1_header[0] = 'N';
        nas1_header[1] = 'A';
        nas1_header[2] = 'S';
        nas1_header[3] = '1';

        // Offset-ul de start al codului este 8 (imediat după antet)
        uint32_t entry_offset = 8;
        nas1_header[4] = (uint8_t)(entry_offset & 0xFF);
        nas1_header[5] = (uint8_t)((entry_offset >> 8) & 0xFF);
        nas1_header[6] = (uint8_t)((entry_offset >> 16) & 0xFF);
        nas1_header[7] = (uint8_t)((entry_offset >> 24) & 0xFF);

        // Scriem antetul NAS1
        fwrite(nas1_header, 1, 8, f);

        // 2. Apelăm TCC să scrie codul mașină generat direct după antet
        int res = tcc_output_file(s, filename);
        fclose(f);
        return res;
    }

    // Pentru alte fișiere (.o, etc.)
    return tcc_output_file(s, filename);
}

int main(int argc, char **argv)
{
    TCCState *s, *s1;
    int ret, opt, n = 0, t = 0, done;
    unsigned start_time = 0, end_time = 0;
    const char *first_file;
    int argc0 = argc;
    char **argv0 = argv;

redo:
    argc = argc0, argv = argv0;
    s = s1 = tcc_new();
    opt = tcc_parse_args(s, &argc, &argv);

    if (n == 0) {
        ret = 0;
        if (opt == OPT_HELP) {
            fputs(help, stdout);
            goto err;
        } else if (s->verbose) {
            nano_printf("%s", version);
        }

        if (opt) {
            if (opt < 0) err:
                ret = 1;
            tcc_delete(s);
            return ret;
        }
        if (s->nb_files == 0) {
            tcc_error_noabort("no input files");
        }
        if (s->nb_errors)
            goto err;
    }

    set_environment(s);
    if (s->output_type == 0)
        s->output_type = TCC_OUTPUT_EXE;

    ret = tcc_set_output_type(s, s->output_type);

    /* Compilează fiecare fișier sursă */
    first_file = NULL;
    while (0 == ret) {
        struct filespec *f = s->files[n];
        s->filetype = f->type;
        if (!first_file)
            first_file = f->name;
        ret = tcc_add_file(s, f->name);
        if (++n == s->nb_files)
            break;
    }

    if (0 == ret) {
        if (!s->outfile)
            s->outfile = default_outputfile(s, first_file);
        
        // Folosim wrapper-ul nostru cu Antet NAS1
        ret = nano_output_file(s, s->outfile);
    }

    tcc_delete(s);
    return ret;
}