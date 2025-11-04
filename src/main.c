#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/file.h"
#include "../lib/compress.h"
#include "../lib/decompress.h"
#include "../lib/data_types.h"
#include "../lib/debugmalloc.h"

int main(int argc, char* argv[]){
    // for (int i = 0; i < argc; i++){
    //     if (strcmp(argv[i], "-x") == 0) printf("Extract");
    //     else if (strcmp(argv[i], "-h") == 0) printf("Usage");
    //     else printf("Invalid");
    // }
    char *data;
    int data_len = read_raw("test.txt", &data);
    if (data_len < 0) {
        fprintf(stderr, "Failed to read file 'test.txt'\n");
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
        printf("File is empty.\n");
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
        fprintf(stderr, "Compression failed.\n");
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
    compressed_file->original_file = "test.txt";
    compressed_file->original_size = data_len;
    compressed_file->file_name = "test.huff";
    write_compressed(compressed_file, false);

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
    remove("test.txt");
    compressed_file = calloc(1, sizeof(Compressed_file));
    read_compressed("test.huff", compressed_file);
    char *raw_data = malloc(compressed_file->original_size * sizeof(char));
    decompress(compressed_file, raw_data);
    write_raw(compressed_file->original_file, raw_data, compressed_file->original_size, false);
    free(raw_data);
    free(compressed_file->file_name);
    free(compressed_file->original_file);
    free(compressed_file->huffman_tree);
    free(compressed_file->compressed_data);
    free(compressed_file);
    return 0;
}
