#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/compress.h"
#include "../lib/decompress.h"
#include "../lib/data_types.h"
#include "../lib/debugmalloc.h"

int main() {
    char *data = "hello world";
    long data_len = strlen(data);

    long *frequencies = calloc(256, sizeof(long));
    count_frequencies(data, data_len, frequencies);

    int leaf_count = 0;
    for (int i = 0; i < 256; i++) {
        if (frequencies[i] != 0) {
            leaf_count++;
        }
    }

    Node *nodes = malloc((2 * leaf_count - 1) * sizeof(Node));
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

    Compressed_file *compressed_file = malloc(sizeof(Compressed_file));
    compress(data, data_len, nodes, root_node, cache, compressed_file);

    compressed_file->huffman_tree = nodes;
    compressed_file->tree_size = tree_size * sizeof(Node);

    char *raw_data = malloc(data_len * sizeof(char));
    decompress(compressed_file, raw_data);

    if (memcmp(data, raw_data, data_len) != 0) {
        printf("Decompression failed!\n");
        printf("Original:  %s\n", data);
        printf("Decompressed: %s\n", raw_data);
    } else {
        printf("Decompression successful!\n");
    }

    free(nodes);
    for(int i=0; i<256; ++i) {
        if (cache[i] != NULL) {
            free(cache[i]);
        }
    }
    free(cache);
    free(compressed_file->compressed_data);
    free(compressed_file);
    free(raw_data);

    return 0;
}
