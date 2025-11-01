#include <stdio.h>
#include <stdlib.h>
#include "../lib/file.h"
#include "../lib/compress.h"
#include "../lib/data_types.h"
#include "../lib/debugmalloc.h"

int main(int argc, char* argv[]){
    for (int i = 0; i < argc; i++){
        if (strcmp(argv[i], "-x") == 0) printf("Extract");
        else if (strcmp(argv[i], "-h") == 0) printf("Usage");
        else printf("Invalid");
    }
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

    sort_nodes(nodes, leaf_count);
    Node *root_node = construct_tree(nodes, leaf_count);

    if (root_node != NULL) {
        long tree_size = (root_node - nodes) + 1;
        Node *resized_nodes = realloc(nodes, tree_size * sizeof(Node));
        if (resized_nodes != NULL) {
            nodes = resized_nodes;
        }
    }


    free(data);
    free(frequencies);
    free(nodes);

    return 0;
}
