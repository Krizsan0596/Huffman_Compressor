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

/*
 * Tomoriteshez szukseges mappa feldolgozas.
 * Bejarja a mappat, archivalja es szerializalja az adatokat.
 * Sikeres muveletek eseten a szerializalt adat hosszat adja vissza, hiba eseten negativ erteket.
 */
static int prepare_directory(char *input_file, char **data, int *directory_size) {
    char current_path[1000];
    char *sep = strrchr(input_file, '/');
    char *parent_dir = NULL;
    char *file_name = NULL;
    Directory_item *archive = NULL;
    int archive_size = 0;
    int current_index = 0;
    int res = 0;
    
    while (true) {
        if (getcwd(current_path, sizeof(current_path)) == NULL) {
            printf("Nem sikerult elmenteni az utat.\n");
            res = DIRECTORY_ERROR;
            break;
        }
        

        if (sep != NULL && !(strncmp(input_file, "./", 2) == 0 || strncmp(input_file, "../", 3) == 0)) {
            if (sep == input_file) {
                parent_dir = strdup("/");
                if (parent_dir == NULL) {
                    res = MALLOC_ERROR;
                    break;
                }
            }
            else {
                int parent_dir_len = sep - input_file;
                parent_dir = malloc(parent_dir_len + 1);
                if (parent_dir == NULL) {
                    res = MALLOC_ERROR;
                    break;
                }
                strncpy(parent_dir, input_file, parent_dir_len);
                parent_dir[parent_dir_len] = '\0';
            }
            file_name = strdup(sep + 1);
            if (file_name == NULL) {
                res = MALLOC_ERROR;
                break;
            }
            if (chdir(parent_dir) != 0) {
                printf("Nem sikerult belepni a mappaba.\n");
                res = DIRECTORY_ERROR;
                break;
            }
        }
        
        *directory_size = archive_directory((sep != NULL) ? file_name : input_file, &archive, &current_index, &archive_size);
        if (*directory_size < 0) {
            if (*directory_size == MALLOC_ERROR) {
                printf("Nem sikerult lefoglalni a memoriat a mappa archivallasakor.\n");
            } else if (*directory_size == DIRECTORY_OPEN_ERROR) {
                printf("Nem sikerult megnyitni a mappat.\n");
            } else if (*directory_size == FILE_READ_ERROR) {
                printf("Nem sikerult beolvasni egy fajlt a mappabol.\n");
            } else {
                printf("Nem sikerult a mappa archivallasa.\n");
            }
            res = *directory_size;
            break;
        }
        
        res = serialize_archive(archive, archive_size, data);
        if (res < 0) {
            if (res == MALLOC_ERROR) {
                printf("Nem sikerult lefoglalni a memoriat a szerializalaskor.\n");
            } else if (res == EMPTY_DIRECTORY) {
                printf("A mappa ures.\n");
            } else {
                printf("Nem sikerult a mappa szerializalasa.\n");
            }
            break;
        }
        
        if (sep != NULL) {
            if (chdir(current_path) != 0) {
                printf("Nem sikerult kilepni a mappabol.\n");
                res = DIRECTORY_ERROR;
                break;
            }
        }
        break;
    }
    
    if (archive_size > 0) {
        for (int i = 0; i < archive_size; ++i) {
            if (archive[i].is_dir) {
                free(archive[i].dir_path);
            } else {
                free(archive[i].file_path);
                free(archive[i].file_data);
            }
        }
    }
    free(archive);
    free(parent_dir);
    free(file_name);
    
    return res;
}

/*
 * Kitomoriteshez szukseges mappa feldolgozas.
 * Deszerializalja es kitomoriti az archivalt mappakat.
 * Sikeres muveletek eseten 0-t, hiba eseten negativ erteket ad vissza.
 */
static int restore_directory(char *raw_data, char *output_file, bool force) {
    Directory_item *archive = NULL;
    int archive_size = 0;
    int res = 0;
    
    while (true) {
        archive_size = deserialize_archive(&archive, raw_data);
        if (archive_size < 0) {
            if (archive_size == MALLOC_ERROR) {
                printf("Nem sikerult lefoglalni a memoriat a beolvasaskor.\n");
            } else {
                printf("Nem sikerult a tomoritett mappa beolvasasa.\n");
            }
            res = archive_size;
            break;
        }
        
        if (output_file != NULL) {
            if (mkdir(output_file, 0755) != 0 && errno != EEXIST) {
                printf("Nem sikerult letrehozni a kimeneti mappat.\n");
                res = MKDIR_ERROR;
                break;
            }
        }
        
        int ret = extract_directory(output_file != NULL ? output_file : ".", archive, archive_size, force);
        if (ret != 0) {
            if (ret == MKDIR_ERROR) {
                printf("Nem sikerult letrehozni egy mappat a kitomoriteskor.\n");
            } else if (ret == FILE_WRITE_ERROR) {
                printf("Nem sikerult kiirni egy fajlt a kitomoriteskor.\n");
            } else {
                printf("Nem sikerult a mappa kitomoritese.\n");
            }
            res = ret;
            break;
        }
        break;
    }
    if (archive_size > 0) {
        for (int i = 0; i < archive_size; ++i) {
            if (archive[i].is_dir) {
                free(archive[i].dir_path);
            } else {
                free(archive[i].file_path);
                free(archive[i].file_data);
            }
        }
    }
    free(archive);
    
    return res;
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
        // Ha nem adott meg kimeneti fajlt a felhasznalo, general egyet.
        bool output_generated = false;
        if (args.output_file == NULL) {
            output_generated = true;
            args.output_file = generate_output_file(args.input_file);
            if (args.output_file == NULL) {
                printf("Nem sikerult lefoglalni a memoriat.\n");
                return ENOMEM;
            }
        }
        char *data;
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
                printf("Nem sikerult megnyitni a fajlt (%s).\n", args.input_file);
                if (output_generated) {
                    free(args.output_file);
                }
                return EIO;
            }
        }

        int write_res = 0;
        long *frequencies = NULL;
        Compressed_file *compressed_file = NULL;
        Node *nodes = NULL;
        long tree_size = 0;
        char **cache = NULL;
        int res = 0;
        
        // A while ciklusbol a vegen garantaltan ki break-elunk, de ha hiba tortenik, akkor a vegere ugrunk.
        while (true) {
            // Megszamolja a bemeneti adat bajtjainak gyakorisagat.
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

            // Felepiti a Huffman fat a rendezett levelek tombjebol. 
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
            
            // Tomoriti a beolvasott adatokat a compressed_file strukturaba.
            int compress_res = compress(data, data_len, nodes, root_node, cache, compressed_file);
            if (compress_res != 0) {
                printf("Nem sikerult a tomorites.\n");
                res = compress_res;
                break;
            }

            if (args.directory) compressed_file->is_dir = true;
            else compressed_file->is_dir = false;

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

        for (int i = 0; i < 256; ++i) {
            if (cache[i] != NULL) {
                free(cache[i]);
            }
        }
        free(cache);
        free(data);
        return res != 0 ? res : write_res;

    /*
     * Kitomoritesi ag: beolvassuk a kapott fajlt, kitomoritunk egy bufferbe,
     * majd az eredeti nevre vagy a megadott kimenetre irjuk ki a kitomoritett adatot.
     */
    } else if (args.extract_mode) {
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

            // A fajlbol beolvassa hogy mappa volt e tomoritve.
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
    else {
        printf("Az egyik modot (-c vagy -x) meg kell adni.\n");
        print_usage(argv[0]);
        return EINVAL;
    }
}
