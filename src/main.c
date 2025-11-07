#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "../lib/file.h"
#include "../lib/compress.h"
#include "../lib/decompress.h"
#include "../lib/data_types.h"
#include "../lib/debugmalloc.h"

static void print_usage(const char *prog_name) {
    const char *usage =
        "Huffman kodolo\n"
        "Hasznalat: %s -c|-x [-o KIMENETI_FAJL] BEMENETI_FAJL\n"
        "\n"
        "Opciok:\n"
        "\t-c                Tomorites\n"
        "\t-x                Kitomorites\n"
        "\t-o KIMENETI_FAJL  Kimeneti fajl megadasa (tomoriteskor, opcionalis).\n"
        "\t-h                Kiirja ezt az utmutatot.\n"
        "\t-f                Ha letezik a KIMENETI_FAJL, kerdes nelkul felulirja."
        "BEMENETI_FAJL: A tomoritendo vagy visszaallitando fajl utvonala.\n"
        "\tA -c es -x kapcsolok kizarjak egymast.";

    printf(usage, prog_name);
}

int main(int argc, char* argv[]){
    bool compress_mode = false;
    bool extract_mode = false;
    bool force = false;
    char *input_file;
    char *output_file;
    for (int i = 1; i < argc; i++){
        if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "-c") == 0) compress_mode = true;
        else if (strcmp(argv[i], "-x") == 0) extract_mode = true;
        else if (strcmp(argv[i], "-f") == 0) force = true;
        else if (strcmp(argv[i], "-o") == 0) {
            if (argv[++i] != NULL) output_file = argv[i];
            else {
                printf("Az -o kapcsolo utan add meg a kimeneti fajlt.\n");
                print_usage(argv[0]);
                return 1;
            }
        }
        else {
            input_file = argv[i];
            FILE *f = fopen(input_file, "r");
            if (f != NULL) {
                fclose(f);
                continue;
            }
            else {
                fclose(f);
                printf("A (%s) fajl nem letezik.\n", input_file);
                print_usage(argv[0]);
                return 1;
            }
        }
    }
    //compression
    char *data;
    int data_len = read_raw(input_file, &data);
    if (data_len < 0) {
        printf("Nem sikerult megnyitni a fajlt (%s).", input_file);
        return 1;
    }

    long *frequencies = calloc(256, sizeof(long));
    if (frequencies == NULL) {
        free(data);
        return 1; // malloc error
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
        return 0;
    }

    Node *nodes = malloc((2 * leaf_count - 1) * sizeof(Node));
    if (nodes == NULL) {
        free(data);
        free(frequencies);
        return 1; // malloc error
    }

    int j = 0;
    for (int i = 0; i < 256; i++) {
        if (frequencies[i] != 0) {
            nodes[j] = construct_leaf(frequencies[i], (char) i);
            j++;
        }
    }
    free(frequencies);

    sort_nodes(nodes, leaf_count);
    Node *root_node = construct_tree(nodes, leaf_count);

    long tree_size;
    if (root_node != NULL) {
        tree_size = (root_node - nodes) + 1;
    }
    else {
        return 2; //tree error
    }
    char **cache = calloc(256, sizeof(char*));
    if (cache == NULL) {
        free(data);
        free(nodes);
        return 1; // malloc error
    }
 
    Compressed_file *compressed_file = malloc(sizeof(Compressed_file));
    if (compressed_file == NULL) {
        free(data);
        free(nodes);
        free(cache);
        return 1; // malloc error
    }

    if (compress(data, data_len, nodes, root_node, cache, compressed_file) != 0) {
        printf("A tomorites nem sikerult.");
        free(data);
        free(nodes);
        for(int i=0; i<256; ++i) free(cache[i]);
        free(cache);
        if (compressed_file->compressed_data != NULL) {
            free(compressed_file->compressed_data);
        }
        free(compressed_file);
        return 1;
    }

    compressed_file->huffman_tree = nodes;
    compressed_file->tree_size = tree_size * sizeof(Node);
    compressed_file->original_file = input_file;
    compressed_file->original_size = data_len;
    compressed_file->file_name = output_file;
    write_compressed(compressed_file, force);

    free(nodes);
    if (compressed_file->compressed_data != NULL) {
        free(compressed_file->compressed_data);
    }

    for(int i=0; i<256; ++i) {
        if (cache[i] != NULL) {
            free(cache[i]);
        }
    }
    free(cache);
    free(data);
    free(compressed_file);
    
    // decompression
    compressed_file = calloc(1, sizeof(Compressed_file));
    if (compressed_file == NULL) {
        printf("Nem sikerult lefoglalni a memoriat.");
        return 1;
    }
    if (compressed_file == NULL) {
        printf("Nem sikerult lefoglalni a memoriat.");
        return 1;
    }
    if (read_compressed("test.huff", compressed_file) != 0) {
        printf("Nem sikerult beovasni a tomoritett fajlt (%s).", input_file);
        free(compressed_file);
        return 1;
    }
    if (compressed_file->original_size <= 0) {
        printf("A tomoritett fajl (%s) serult, nem sikerult beolvasni.", input_file);
        free(compressed_file->file_name);
        free(compressed_file->original_file);
        free(compressed_file->huffman_tree);
        free(compressed_file->compressed_data);
        free(compressed_file);
        return 1;
    }
    char *raw_data = malloc(compressed_file->original_size * sizeof(char));
    if (raw_data == NULL) {
        printf("Nem sikerult lefoglalni a memoriat.");
        free(compressed_file->file_name);
        free(compressed_file->original_file);
        free(compressed_file->huffman_tree);
        free(compressed_file->compressed_data);
        free(compressed_file);
        return 1;
    }
    int decompress_result = decompress(compressed_file, raw_data);
    if (decompress_result != 0) {
        printf("Nem sikerult a kitomorites. ");
        free(raw_data);
        free(compressed_file->file_name);
        free(compressed_file->original_file);
        free(compressed_file->huffman_tree);
        free(compressed_file->compressed_data);
        free(compressed_file);
        return 1;
    }
    if (write_raw(compressed_file->original_file, raw_data, compressed_file->original_size, false) < 0) {
        printf("Hiba tortent a kimeneti fajl (%s) irasa kozben. \n", compressed_file->original_file);
        free(raw_data);
        free(compressed_file->file_name);
        free(compressed_file->original_file);
        free(compressed_file->huffman_tree);
        free(compressed_file->compressed_data);
        free(compressed_file);
        return 1;
    }
    free(raw_data);
    free(compressed_file->file_name);
    free(compressed_file->original_file);
    free(compressed_file->huffman_tree);
    free(compressed_file->compressed_data);
    free(compressed_file);
    return 0;
}
