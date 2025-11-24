
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "../lib/file.h"
#include "../lib/compress.h"
#include "../lib/decompress.h"
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
        "\tBEMENETI_FAJL: A tomoritendo vagy visszaallitando fajl utvonala.\n"
        "\tA -c es -x kapcsolok kizarjak egymast.";

    printf(usage, prog_name);
}

int main(int argc, char* argv[]){
    bool compress_mode = false;
    bool extract_mode = false;
    bool force = false;
    char *input_file = NULL;
    char *output_file = NULL;

    /* 
     * Parancssori opciok feldolgozasa: egy mod valaszthato, az -o a kimenetet, az -f a felulirast kezeli.
     * Az elso nem kapcsolos argumentum lesz a bemeneti fajl.
     */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'h':
                    print_usage(argv[0]);
                    return SUCCESS;
                case 'c':
                    compress_mode = true;
                    break;
                case 'x':
                    extract_mode = true;
                    break;
                case 'f':
                    force = true;
                    break;
                case 'o':
                    if (++i < argc) {
                        output_file = argv[i];
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
            if (input_file == NULL) {
                input_file = argv[i];
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
    if (input_file == NULL) {
        printf("Nem lett bemeneti fajl megadva.\n");
        print_usage(argv[0]);
        return EINVAL;
    }
    
    FILE *f = fopen(input_file, "r");
    if (f == NULL) {
        printf("A (%s) fajl nem nyithato meg.\n", input_file);
        print_usage(argv[0]);
        return ENOENT;
    }
    fclose(f);

    if (compress_mode && extract_mode) {
        printf("A -c es -x kapcsolok kizarjak egymast.\n");
        print_usage(argv[0]);
        return EINVAL;
    }

    
    if (compress_mode) {
        // Ha nem adott meg kimeneti fajt a felhasznalo, general egyet.
        bool output_default = false;
        if (output_file == NULL) {
            output_default = true;
            output_file = generate_output_file(input_file);
            if (output_file == NULL) {
                printf("Nem sikerult lefoglalni a memoriat.");
                return ENOMEM;
            }
        }
        
        int res = run_compression(input_file, output_file, force, output_default);
        return res;

    /*
     * Kitomoritesi ag: beolvassuk a kapott fajlt, kitomoritunk egy bufferbe,
     * majd az eredeti nevre vagy a megadott kimenetre irjuk ki a kitomoritett adatot.
     */
    } else if (extract_mode) {
        int res = run_decompression(input_file, output_file, force);
        return res;
    } 
    else {
        printf("Az egyik modot (-c vagy -x) meg kell adni.");
        print_usage(argv[0]);
        return EINVAL;
    }
}
