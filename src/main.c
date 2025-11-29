#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../lib/file.h"
#include "../lib/compress.h"
#include "../lib/decompress.h"
#include "../lib/directory.h"
#include "../lib/data_types.h"
#include "../lib/debugmalloc.h"

/*
 * Kiirja a program hasznalati utasitasat.
 * Segitseg vagy ervenytelen kapcsolok eseten hivjuk meg.
 */
static void print_usage(const char *prog_name) {
    const char *usage =
        "Huffman kodolo\n"
        "Hasznalat: %s -c|-x [-o KIMENETI_FAJL] BEMENETI_FAJL\n"
        "\n"
        "Opciok:\n"
        "\t-c                Tomorites\n"
        "\t-x                Kitomorites\n"
        "\t-o KIMENETI_FAJL  Kimeneti fajl megadasa (opcionalis).\n"
        "\t-h                Kiirja ezt az utmutatot.\n"
        "\t-f                Ha letezik a KIMENETI_FAJL, kerdes nelkul felulirja.\n"
        "\t-r                Rekurzivan egy megadott mappat tomorit (csak tomoriteskor szukseges).\n"
        "\tBEMENETI_FAJL: A tomoritendo vagy visszaallitando fajl utvonala.\n"
        "\tA -c es -x kapcsolok kizarjak egymast.";

    printf(usage, prog_name);
}

/* 
 * Parancssori opciok feldolgozasa: egy mod valaszthato, az -o a kimenetet, az -f a felulirast kezeli.
 * Az elso nem kapcsolos argumentum lesz a bemeneti fajl.
 */
static int parse_arguments(int argc, char* argv[], Arguments *args) {
    args->compress_mode = false;
    args->extract_mode = false;
    args->force = false;
    args->directory = false;
    args->input_file = NULL;
    args->output_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'h':
                    print_usage(argv[0]);
                    return HELP_REQUESTED;
                case 'c':
                    args->compress_mode = true;
                    break;
                case 'x':
                    args->extract_mode = true;
                    break;
                case 'f':
                    args->force = true;
                    break;
                case 'r':
                    args->directory = true;
                    break;
                case 'o':
                    if (++i < argc) {
                        args->output_file = argv[i];
                    } else {
                        printf("Az -o kapcsolo utan add meg a kimeneti fajlt.\n");
                        print_usage(argv[0]);
                        return EINVAL;
                    }
                    break;
                default:
                    printf("Ismeretlen kapcsolo: %s\n", argv[i]);
                    print_usage(argv[0]);
                    return EINVAL;
            }
        } else {
            if (args->input_file == NULL) {
                args->input_file = argv[i];
            } else {
                printf("Tobb bemeneti fajl lett megadva.\n");
                print_usage(argv[0]);
                return EINVAL;
            }
        }
    }

    /*
     * Ellenorizzuk, hogy megadtak-e a bemeneti fajlt, majd leellenorizzuk, hogy olvashato-e.
     * Ha nem, kilepunk a programbol.
     */
    if (args->input_file == NULL) {
        printf("Nem lett bemeneti fajl megadva.\n");
        print_usage(argv[0]);
        return EINVAL;
    }
    
    FILE *f = fopen(args->input_file, "r");
    if (f == NULL) {
        printf("A (%s) fajl nem nyithato meg.\n", args->input_file);
        print_usage(argv[0]);
        return ENOENT;
    }
    fclose(f);

    if (args->compress_mode && args->extract_mode) {
        printf("A -c es -x kapcsolok kizarjak egymast.\n");
        print_usage(argv[0]);
        return EINVAL;
    }

    return 0;
}

int main(int argc, char* argv[]){
    Arguments args;
    int parse_result = parse_arguments(argc, argv, &args);
    if (parse_result == HELP_REQUESTED) {
        return SUCCESS;
    }
    if (parse_result != SUCCESS) {
        return parse_result;
    }

    if (args.directory) {
        struct stat st;
        int ret = stat(args.input_file, &st);
        if (ret != 0) {
            printf("Nem sikerult ellenorizni a mappat.\n");
            return ret;
        }
        else if (S_ISREG(st.st_mode)) args.directory = false;
    }
    else {
        struct stat st;
        int ret = stat(args.input_file, &st);
        if (ret != 0) {
            printf("Nem sikerult ellenorizni a fajlt.\n");
            return ret;
        }
        else if (S_ISDIR(st.st_mode)) {
            printf("Az -r kapcsolo nelkul nem tomorit mappat a program.\n");
            print_usage(argv[0]);
            return EISDIR;
        }
    }
    
    if (args.compress_mode) {
        return run_compression(args);
    } else if (args.extract_mode) {
        return run_decompression(args);
    } 
    else {
        printf("Az egyik modot (-c vagy -x) meg kell adni.\n");
        print_usage(argv[0]);
        return EINVAL;
    }
}
