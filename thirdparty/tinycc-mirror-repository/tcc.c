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
        strcpy(buf, "a.nas1"); // Executabilele Nano OS folosesc formatul NAS1
    return tcc_strdup(buf);
}

// Timer simplificat pentru Nano OS
static unsigned getclock_ms(void)
{
    return 0; // Sau poți folosi un counter/ticks din kernelul Nano OS
}

static int nano_add_startup(TCCState *s)
{
    static const char startup[] =
        "extern int main(void);\n"
        "void _start(void)\n"
        "{\n"
        "    long status = main();\n"
        "    asm volatile(\"mov %0, %%rdi\\n\"\n"
        "                 \"mov $20, %%rax\\n\"\n"
        "                 \"int $0x80\\n\"\n"
        "                 : : \"r\"(status) : \"rax\", \"rdi\");\n"
        "    for (;;) {}\n"
        "}";

    s->filetype = AFF_TYPE_C;
    return tcc_compile_string(s, startup);
}

// Executabilele Nano OS sunt imagini binare cu antet NAS1, nu executabile ELF.
static int nano_output_file(TCCState *s, const char *filename) {
    if (s->output_type == TCC_OUTPUT_EXE) {
        s->output_format = TCC_OUTPUT_FORMAT_BINARY;
        s->has_text_addr = 1;
        s->text_addr = 0x800008;
        nano_tcc_set_nas1_output(1);
    }

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

    nano_tcc_set_io_debug(s->verbose != 0);
    set_environment(s);
    if (s->output_type == 0)
        s->output_type = TCC_OUTPUT_EXE;

    if (s->output_type == TCC_OUTPUT_EXE) {
        s->nostdlib = 1;
        s->nostdlib_paths = 1;
        s->static_link = 1;
    }

    ret = tcc_set_output_type(s, s->output_type);

    if (ret == 0 && s->output_type == TCC_OUTPUT_EXE) {
        ret = nano_add_startup(s);
    }

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