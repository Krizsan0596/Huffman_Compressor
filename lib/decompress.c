#include "debugmalloc.h"
#include "data_types.h"
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "file.h"
#include "decompress.h"
#include "directory.h"

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

        /* A tarolt flag jelzi, hogy mappa volt-e eredetileg: ez hatarozza meg a kimeneti utvonal felepitest. */
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
            int ret = restore_directory(raw_data, args.output_file, args.force, args.no_preserve_perms);
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
