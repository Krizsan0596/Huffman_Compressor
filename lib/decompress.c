#include "debugmalloc.h"
#include "data_types.h"
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include "file.h"
#include "decompress.h"

/*
 * A Huffman fat bejarva ujra eloallitja az eredeti adatokat bitrol bitre.
 * A tomoritett bufferbol olvas, es a kitomoritett bajtokat a hivo altal adott tombbe irja.
 * Sikeres kitomorites eseten 0-t, hibak eseten negativ szamokat ad vissza.
 */
int decompress(Compressed_file *compressed, char *raw) {
    long root_index = (compressed->tree_size / sizeof(Node)) - 1;

    if (root_index < 0) {
        return TREE_ERROR;
    }

    long current_node = root_index;
    long current_raw = 0;

    unsigned char buffer = 0;
    for (long i = 0; i < compressed->data_size; i++) {
        if (current_raw >= compressed->original_size) {
            break;
        }

        if (i % 8 == 0) {
            buffer = compressed->compressed_data[i / 8];
        }

        if (buffer & (1 << (7 - i % 8))) {
            current_node = compressed->huffman_tree[current_node].right;
        } else {
            current_node = compressed->huffman_tree[current_node].left;
        }

        if (compressed->huffman_tree[current_node].type == LEAF) {
            raw[current_raw++] = compressed->huffman_tree[current_node].data;
            current_node = root_index;
        }
    }

    return 0;
}

/*
 * Kitomorites: beolvassuk a kapott fajlt, kitomoritunk egy bufferbe,
 * majd az eredeti nevre vagy a megadott kimenetre irjuk ki a kitomoritett adatot.
 */
int run_decompression(char *input_file, char *output_file, bool force) {
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
