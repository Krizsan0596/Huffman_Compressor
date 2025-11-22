#include "debugmalloc.h"
#include "data_types.h"
#include <string.h>
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
