#include "debugmalloc.h"
#include "data_types.h"
#include <string.h>
#include "decompress.h"

static char get_leaf(char *buffer, char **compressed_data, Node *nodes, long root_index, long *total_bits) {
    long current_node = root_index;
    short bit_count = *total_bits % 8;
    while (true){
        if (nodes[current_node].type == LEAF) return nodes[current_node].data;
        if (bit_count == 8) {
            bit_count = 0;
            memcpy(buffer, *compressed_data, sizeof(char));
            (*compressed_data)++;
        }
        if ((1 << (7 - bit_count)) & *buffer) {
            current_node = nodes[current_node].right;
        }
        else {
            current_node = nodes[current_node].left;
        }
        bit_count++;
        (*total_bits)++;
    }
}

int decompress(Compressed_file *compressed, char *raw) {
    char buffer = 0;
    char leaf;
    char *current_compressed = compressed->compressed_data;
    char *current_raw = raw;
    long total_bits = 0;
    memcpy(&buffer, current_compressed++, sizeof(char));
    while (total_bits < compressed->data_size) {
        char leaf = get_leaf(&buffer, &current_compressed, compressed->huffman_tree, (compressed->tree_size/sizeof(Node)) - 1, &total_bits);
        memcpy(current_raw, &leaf, sizeof(char));
        current_raw++;
    }
    return 0;
}
