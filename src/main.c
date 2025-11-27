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

int main(int argc, char* argv[]){
    bool compress_mode = false;
    bool extract_mode = false;
    bool force = false;
    bool directory = false;
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
                case 'r':
                    directory = true;
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
    if (directory) {
        struct stat st;
        int ret = stat(input_file, &st);
        if (ret != 0) {
            printf("Nem sikerult ellenorizni a mappat.\n");
            return ret;
        }
        else if (S_ISREG(st.st_mode)) directory = false;
    }
    else {
        struct stat st;
    int ret = stat(input_file, &st);
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
    
    if (compress_mode) {
        // Ha nem adott meg kimeneti fajt a felhasznalo, general egyet.
        bool output_generated = false;
        if (output_file == NULL) {
            output_generated = true;
            output_file = generate_output_file(input_file);
            if (output_file == NULL) {
                printf("Nem sikerult lefoglalni a memoriat.\n");
                return ENOMEM;
            }
        }
        char *data;
        int data_len = 0;
        int directory_size = 0;
        Directory_item *archive = NULL;
        int archive_size = 0;
        int current_index = 0;

        if (directory) {
            char current_path[1000];
            if (getcwd(current_path, sizeof(current_path)) == NULL) {
                printf("Nem sikerult elmenteni az utat.\n");
                return errno;
            }
            char *sep = strrchr(input_file, '/');
            char *parent_dir = NULL;
            char *file_name = NULL;
            if (sep != NULL) {
                int parent_dir_len = sep - input_file;
                parent_dir = malloc(parent_dir_len + 1);
                strncpy(parent_dir, input_file, parent_dir_len);
                parent_dir[parent_dir_len] = '\0';
                file_name = malloc(strlen(input_file) - parent_dir_len + 1);
                strncpy(file_name, sep + 1, strlen(input_file) - parent_dir_len);
                file_name[strlen(input_file) - parent_dir_len] = '\0';
                if (chdir(parent_dir) != 0) {
                    printf("Nem sikerult belepni a mappaba.\n");
                    return errno;
                }
            }
            directory_size = archive_directory((sep != NULL) ? file_name : input_file, &archive, &current_index, &archive_size);
            if (directory_size < 0) {
                if (directory_size == MALLOC_ERROR) {
                    printf("Nem sikerult lefoglalni a memoriat a mappa archivallasakor.\n");
                } else if (directory_size == DIRECTORY_OPEN_ERROR) {
                    printf("Nem sikerult megnyitni a mappat.\n");
                } else if (directory_size == FILE_READ_ERROR) {
                    printf("Nem sikerult beolvasni egy fajlt a mappabol.\n");
                } else {
                    printf("Nem sikerult a mappa archivallasa.\n");
                }
                return directory_size;
            }
            data_len = serialize_archive(archive, archive_size, &data);
            if (data_len < 0) {
                if (data_len == MALLOC_ERROR) {
                    printf("Nem sikerult lefoglalni a memoriat a szerializalaskor.\n");
                } else if (data_len == EMPTY_DIRECTORY) {
                    printf("A mappa ures.\n");
                } else {
                    printf("Nem sikerult a mappa szerializalasa.\n");
                }
                return data_len;
            }
            if (sep != NULL) {
                if (chdir(current_path) != 0) {
                    printf("Nem sikerult kilepni a mappaba.\n");
                    return errno;
                }
            }
            free(parent_dir);
            free(file_name);
        }
        else {
            data_len = read_raw(input_file, &data);
            if (data_len < 0) {
                printf("Nem sikerult megnyitni a fajlt (%s).\n", input_file);
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
                printf("A fajl (%s) ures.\n", input_file);
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

            if (directory) compressed_file->is_dir = true;
            else compressed_file->is_dir = false;

            compressed_file->huffman_tree = nodes;
            compressed_file->tree_size = tree_size * sizeof(Node);
            compressed_file->original_file = input_file;
            compressed_file->original_size = data_len;
            compressed_file->file_name = output_file;
            write_res = write_compressed(compressed_file, force);
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
                                                    (double)write_res/(directory ? directory_size : data_len) * 100);
            }
            break;
        }
        free(frequencies);
        if (output_generated) free(output_file);
        free(nodes);
        if (compressed_file != NULL) {
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
        for (int i = 0; i < archive_size; ++i) {
            if (archive[i].is_dir) {
                free(archive[i].dir_path);
            } else {
                free(archive[i].file_path);
                free(archive[i].file_data);
            }
        }
        free(archive);
        return res;

    /*
     * Kitomoritesi ag: beolvassuk a kapott fajlt, kitomoritunk egy bufferbe,
     * majd az eredeti nevre vagy a megadott kimenetre irjuk ki a kitomoritett adatot.
     */
    } else if (extract_mode) {
        Compressed_file *compressed_file;
        char *raw_data = NULL;
        int res = 0;
        while (true) {
            compressed_file = calloc(1, sizeof(Compressed_file));
            if (compressed_file == NULL) {
                printf("Nem sikerult lefoglalni a memoriat.\n");
                res = ENOMEM;
                break;
            }
            
            int read_res = read_compressed(input_file, compressed_file);
            if (read_res != 0) {
                if (read_res == FILE_MAGIC_ERROR) {
                    printf("A tomoritett fajl (%s) serult, nem sikerult beolvasni.\n", input_file);
                    res = EBADF;
                    break;
                }
                printf("Nem sikerult beolvasni a tomoritett fajlt (%s).\n", input_file);
                res = EIO;
                break;
            }

            // A fajlbol beolvassa hogy mappa volt e tomoritve.
            directory = compressed_file->is_dir;
            
            if (compressed_file->original_size <= 0) {
                printf("A tomoritett fajl (%s) serult, nem sikerult beolvasni.\n", input_file);
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

            Directory_item *archive = NULL;
            int archive_size = 0;
            if (directory) {
                archive_size = deserialize_archive(&archive, raw_data);
                if (archive_size < 0) {
                    if (archive_size == MALLOC_ERROR) {
                        printf("Nem sikerult lefoglalni a memoriat a beolvasaskor.\n");
                    } else {
                        printf("Nem sikerult a tomoritett mappa beolvasasa.\n");
                    }
                    res = EIO;
                    break;
                } 
                if (output_file != NULL) {
                    if (mkdir(output_file, 0755) != 0 && errno != EEXIST) {
                        printf("Nem sikerult letrehozni a kimeneti mappat.\n");
                        res = EIO;
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
                    res = EIO;
                    break;
                }
                for (int i = 0; i < archive_size; ++i) {
                    if (archive[i].is_dir) {
                        free(archive[i].dir_path);
                    } else {
                        free(archive[i].file_path);
                        free(archive[i].file_data);
                    }
                }
                free(archive);
                if (res != 0) break;
            }

            else {
                int write_res = write_raw(output_file != NULL ? output_file : compressed_file->original_file, raw_data, compressed_file->original_size, force);
                if (write_res < 0) {
                    printf("Hiba tortent a kimeneti fajl (%s) irasa kozben.\n", output_file != NULL ? output_file : compressed_file->original_file);
                    res = EIO;
                    break;
                }
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
        printf("Az egyik modot (-c vagy -x) meg kell adni.\n");
        print_usage(argv[0]);
        return EINVAL;
    }
}
