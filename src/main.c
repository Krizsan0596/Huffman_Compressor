
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
                    return 0;
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
        printf("Nem lett bemeneti fajl megadva vagy nem olvashato.\n");
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

        char *data;
        int data_len = read_raw(input_file, &data);
        if (data_len < 0) {
            printf("Nem sikerult megnyitni a fajlt (%s).", input_file);
            return EIO;
        }
        
        long *frequencies;
        Compressed_file *compressed_file;
        Node *nodes;
        long tree_size;
        char **cache;
        int res = 0;
        
        // A while ciklusbol a vegen garantaltan ki break-elunk, de ha hiba tortenik, akkor a vegere ugrunk.
        while (true) {
            // Megszamolja a bemeneti adat bajtjainak gyakorisagat.
            frequencies = calloc(256, sizeof(long));
            if (frequencies == NULL) {
                res = MALLOC_ERROR;
                break;
            }
            count_frequencies(data, data_len, frequencies);

            int leaf_count = 0;
            for (int i = 0; i < 256; i++) {
                if (frequencies[i] != 0) {
                    leaf_count++;
                }
            }

            if (leaf_count == 0) {
                free(data);
                free(frequencies);
                printf("A fajl (%s) ures.", input_file);
                res = SUCCESS;
                break;
            }

            nodes = malloc((2 * leaf_count - 1) * sizeof(Node));
            if (nodes == NULL) {
                res = MALLOC_ERROR;
                break;
            }

            int j = 0;
            for (int i = 0; i < 256; i++) {
                if (frequencies[i] != 0) {
                    nodes[j] = construct_leaf(frequencies[i], (char)i);
                    j++;
                }
            }
            free(frequencies);
            frequencies = NULL;

            // Felepiti a Huffman fat a rendezett levelek tombjebol. 
            sort_nodes(nodes, leaf_count);
            Node *root_node = construct_tree(nodes, leaf_count);

            if (root_node != NULL) {
                tree_size = (root_node - nodes) + 1;
            } else {
                res = TREE_ERROR;
                break;
            }
            cache = calloc(256, sizeof(char *));
            if (cache == NULL) {
                res = MALLOC_ERROR;
                break;
            }

            compressed_file = malloc(sizeof(Compressed_file));
            if (compressed_file == NULL) {
                res = MALLOC_ERROR;
                break;
            }
            
            // Tomoriti a beolvasott adatokat a compressed_file strukturaba.
            int compress_res = compress(data, data_len, nodes, root_node, cache, compressed_file);
            if (compress_res != 0) {
                res = compress_res;
                break;
            }
            break;
        }

        compressed_file->huffman_tree = nodes;
        compressed_file->tree_size = tree_size * sizeof(Node);
        compressed_file->original_file = input_file;
        compressed_file->original_size = data_len;
        compressed_file->file_name = output_file;
        int write_res = write_compressed(compressed_file, force);
        if (write_res < 0) {
            if (write_res == NO_OVERWRITE) {
                printf("A fajlt nem irtam felul, nem tortent meg a tomorites.\n");
                write_res = ECANCELED;
            } else {
                printf("Nem sikerult kiirni a kimeneti fajlt (%s).\n", compressed_file->file_name);
                write_res = EIO;
            }
        }

        free(frequencies);
        if (output_default) free(output_file);
        free(nodes);
        if (compressed_file->compressed_data != NULL) {
            free(compressed_file->compressed_data);
        }

        for (int i = 0; i < 256; ++i) {
            if (cache[i] != NULL) {
                free(cache[i]);
            }
        }
        free(cache);
        free(data);
        free(compressed_file);
        return write_res;

    /*
     * Kitomoritesi ag: beolvassuk a kapott fajlt, kitomoritunk egy bufferbe,
     * majd az eredeti nevre vagy a megadott kimenetre irjuk ki a kitomoritett adatot.
     */
    } else if (extract_mode) {
        Compressed_file *compressed_file;
        char *raw_data;
        int res = 0;
        while (true) {
            compressed_file = calloc(1, sizeof(Compressed_file));
            if (compressed_file == NULL) {
                printf("Nem sikerult lefoglalni a memoriat.");
                res = ENOMEM;
                break;
            }
            int read_res = read_compressed(input_file, compressed_file);
            if (read_res != 0) {
                if (read_res == FILE_MAGIC_ERROR) {
                    printf("A tomoritett fajl (%s) serult, nem sikerult beolvasni.", input_file);
                    res = EBADF;
                    break;
                }
                printf("Nem sikerult beolvasni a tomoritett fajlt (%s).", input_file);
                res = EIO;
                break;
            }
            if (compressed_file->original_size <= 0) {
                printf("A tomoritett fajl (%s) serult, nem sikerult beolvasni.", input_file);
                res = EINVAL;
                break;
            }
            raw_data = malloc(compressed_file->original_size * sizeof(char));
            if (raw_data == NULL) {
                printf("Nem sikerult lefoglalni a memoriat.");
                res = ENOMEM;
                break;
            }
            int decompress_result = decompress(compressed_file, raw_data);
            if (decompress_result != 0) {
                printf("Nem sikerult a kitomorites. ");
                res = EIO;
                break;
            }
            int write_res = write_raw(output_file != NULL ? output_file : compressed_file->original_file, raw_data, compressed_file->original_size, force);
            if (write_res < 0) {
                printf("Hiba tortent a kimeneti fajl (%s) irasa kozben. \n", output_file != NULL ? output_file : compressed_file->original_file);
                res = EIO;
                break;
            }
            break;
        }
        free(raw_data);
        free(compressed_file->file_name);
        free(compressed_file->original_file);
        free(compressed_file->huffman_tree);
        free(compressed_file->compressed_data);
        free(compressed_file);
        return res;
    } 
    else {
        print_usage(argv[0]);
        return EINVAL;
    }
}
