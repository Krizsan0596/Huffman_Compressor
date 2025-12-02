#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <math.h>
#include "util.h"
#include "data_types.h"
#include "file.h"
#include "huffman.h"
#include "directory.h"
#include "debugmalloc.h"

/*
 * Kiirja a program hasznalati utasitasat.
 * Segitseg vagy ervenytelen kapcsolok eseten hivjuk meg.
 */
void print_usage(const char *prog_name) {
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
int parse_arguments(int argc, char* argv[], Arguments *args) {
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
     * Ellenorizzuk, hogy megadtak-e a bemeneti fajlt, majd leellenorizzuk, hogy letezik-e.
     * Ha nem, kilepunk a programbol. A stat()-ot hasznaljuk, mert az fopen() nem mukodik mappakon.
     */
    if (args->input_file == NULL) {
        printf("Nem lett bemeneti fajl megadva.\n");
        print_usage(argv[0]);
        return EINVAL;
    }
    
    struct stat st;
    if (stat(args->input_file, &st) != 0) {
        printf("A (%s) fajl nem talalhato.\n", args->input_file);
        print_usage(argv[0]);
        return FILE_READ_ERROR;
    }

    if (args->compress_mode && args->extract_mode) {
        printf("A -c es -x kapcsolok kizarjak egymast.\n");
        print_usage(argv[0]);
        return EINVAL;
    }

    return SUCCESS;
}

/*
 * Elkesziti a kimeneti fajl nevet: ha van kiterjesztes, kicsereli .huff-ra, kulonben hozzaadja.
 * Siker eseten lefoglalt karakterlancot ad vissza, hiba eseten NULL-t.
 */
char* generate_output_file(char *input_file){
    char *dir_end = strrchr(input_file, '/');
    char *name_end;
    if (dir_end != NULL) name_end = strrchr(dir_end, '.');
    else name_end = strrchr(input_file, '.');

    char *out;
    if (name_end != NULL) {
        int name_len = name_end - input_file;
        out = malloc(name_len + 6);
        if (out == NULL) {
            return NULL;
        }
        strncpy(out, input_file, name_len);
        out[name_len] = '\0';
        strcat(out, ".huff");
    }
    else {
        out = malloc(strlen(input_file) + 6);
        if (out == NULL) {
            return NULL;
        }
        strcpy(out, input_file);

        strcat(out, ".huff");
    }
    return out;
}

/*
 * A bemeneti fajlt vagy mappat beolvassa, felepit egy Huffman fat es kiirja a tomoritett adatot.
 * Siker eseten 0-t, hiba eseten negativ hibakodot ad vissza.
 */
int run_compression(Arguments args) {
    bool output_generated = false;
    if (args.output_file == NULL) {
        output_generated = true;
        args.output_file = generate_output_file(args.input_file);
        if (args.output_file == NULL) {
            printf("Nem sikerult lefoglalni a memoriat.\n");
            return ENOMEM;
        }
    }
    char *data = NULL;
    int data_len = 0;
    int directory_size = 0;

    if (args.directory) {
        data_len = prepare_directory(args.input_file, &data, &directory_size);
        if (data_len < 0) {
            if (output_generated) {
                free(args.output_file);
            }
            return data_len;
        }
    }
    else {
        data_len = read_raw(args.input_file, &data);
        if (data_len < 0) {
            if (data_len == EMPTY_FILE) {
                printf("A fajl (%s) ures.\n", args.input_file);
            } else {
                printf("Nem sikerult megnyitni a fajlt (%s).\n", args.input_file);
            }
            if (output_generated) {
                free(args.output_file);
            }
            return data_len;
        }
    }

    int write_res = 0;
    long *frequencies = NULL;
    Compressed_file *compressed_file = NULL;
    Node *nodes = NULL;
    long tree_size = 0;
    char **cache = NULL;
    int res = 0;
    
    while (true) {
        frequencies = calloc(256, sizeof(long));
        if (frequencies == NULL) {
            printf("Nem sikerult lefoglalni a memoriat.\n");
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
            printf("A fajl (%s) ures.\n", args.input_file);
            res = SUCCESS;
            break;
        }

        nodes = malloc((2 * leaf_count - 1) * sizeof(Node));
        if (nodes == NULL) {
            printf("Nem sikerult lefoglalni a memoriat.\n");
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

        sort_nodes(nodes, leaf_count);
        Node *root_node = construct_tree(nodes, leaf_count);

        if (root_node != NULL) {
            tree_size = (root_node - nodes) + 1;
        } else {
            printf("Nem sikerult a Huffman fa felepitese.\n");
            res = TREE_ERROR;
            break;
        }
        cache = calloc(256, sizeof(char *));
        if (cache == NULL) {
            printf("Nem sikerult lefoglalni a memoriat.\n");
            res = MALLOC_ERROR;
            break;
        }

        compressed_file = malloc(sizeof(Compressed_file));
        if (compressed_file == NULL) {
            printf("Nem sikerult lefoglalni a memoriat.\n");
            res = MALLOC_ERROR;
            break;
        }
        
        int compress_res = compress(data, data_len, nodes, root_node, cache, compressed_file);
        if (compress_res != 0) {
            printf("Nem sikerult a tomorites.\n");
            res = compress_res;
            break;
        }

        compressed_file->is_dir = args.directory;

        compressed_file->huffman_tree = nodes;
        compressed_file->tree_size = tree_size * sizeof(Node);
        compressed_file->original_file = args.input_file;
        compressed_file->original_size = data_len;
        compressed_file->file_name = args.output_file;
        write_res = write_compressed(compressed_file, args.force);
        if (write_res < 0) {
            if (write_res == NO_OVERWRITE) {
                printf("A fajlt nem irtam felul, nem tortent meg a tomorites.\n");
                write_res = ECANCELED;
            } else {
                printf("Nem sikerult kiirni a kimeneti fajlt (%s).\n", compressed_file->file_name);
                write_res = EIO;
            }
        }
        else {
            printf("Tomorites kesz.\n"
                    "Eredeti meret:    %d%s\n"
                    "Tomoritett meret: %d%s\n"
                    "Tomorites aranya: %.2f%%\n", data_len, get_unit(&data_len), 
                                                write_res, get_unit(&write_res), 
                                                (double)write_res/(args.directory ? directory_size : data_len) * 100);
        }
        break;
    }
    free(frequencies);
    if (output_generated) free(args.output_file);
    free(nodes);
    if (compressed_file != NULL) {
        free(compressed_file->compressed_data);
        free(compressed_file);
    }

    if (cache != NULL) {
        for (int i = 0; i < 256; ++i) {
            if (cache[i] != NULL) {
                free(cache[i]);
            }
        }
        free(cache);
    }
    free(data);
    if (write_res < 0) res = write_res;
    return res;
}

/*
 * Beolvassa a tomoritett fajlt, dekodolja a Huffman adatokat es kiirja a kapott tartalmat.
 * Mappa eseten letrehozza a fajlszerkezetet, siker eseten 0-val ter vissza.
 */
int run_decompression(Arguments args) {
    Compressed_file *compressed_file = NULL;
    char *raw_data = NULL;
    int res = 0;
    
    while (true) {
        compressed_file = calloc(1, sizeof(Compressed_file));
        if (compressed_file == NULL) {
            printf("Nem sikerult lefoglalni a memoriat.\n");
            res = ENOMEM;
            break;
        }
        
        int read_res = read_compressed(args.input_file, compressed_file);
        if (read_res != 0) {
            if (read_res == FILE_MAGIC_ERROR) {
                printf("A tomoritett fajl (%s) serult, nem sikerult beolvasni.\n", args.input_file);
                res = EBADF;
                break;
            }
            printf("Nem sikerult beolvasni a tomoritett fajlt (%s).\n", args.input_file);
            res = EIO;
            break;
        }

        args.directory = compressed_file->is_dir;
        
        if (compressed_file->original_size <= 0) {
            printf("A tomoritett fajl (%s) serult, nem sikerult beolvasni.\n", args.input_file);
            res = EINVAL;
            break;
        }
        
        raw_data = malloc(compressed_file->original_size * sizeof(char));
        if (raw_data == NULL) {
            printf("Nem sikerult lefoglalni a memoriat.\n");
            res = ENOMEM;
            break;
        }
        
        int decompress_result = decompress(compressed_file, raw_data);
        if (decompress_result != 0) {
            printf("Nem sikerult a kitomorites.\n");
            res = EIO;
            break;
        }

        if (args.directory) {
            int ret = restore_directory(raw_data, args.output_file, args.force);
            if (ret != 0) {
                res = ret;
                break;
            }
        }
        else {
            int write_res = write_raw(args.output_file != NULL ? args.output_file : compressed_file->original_file, raw_data, compressed_file->original_size, args.force);
            if (write_res < 0) {
                printf("Hiba tortent a kimeneti fajl (%s) irasa kozben.\n", args.output_file != NULL ? args.output_file : compressed_file->original_file);
                res = EIO;
                break;
            }
        }
        break;
    }
    
    free(raw_data);
    if (compressed_file != NULL) {
        free(compressed_file->file_name);
        free(compressed_file->original_file);
        free(compressed_file->huffman_tree);
        free(compressed_file->compressed_data);
        free(compressed_file);
    }
    return res;
}

