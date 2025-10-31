#include <stdio.h>
#include <stdlib.h>
#include "../lib/file.h"
#include "../lib/compress.h"
#include "../lib/data_types.h"
#include "../lib/debugmalloc.h"

int main(int argc, char* argv[]){
    // for (int i = 0; i < argc; i++){
    //     if (strcmp(argv[i], "-x") == 0) printf("Extract");
    //     else if (strcmp(argv[i], "-h") == 0) printf("Usage");
    //     else printf("Invalid");
    // }
    char *data;
    int res = read_raw("test.txt", &data);
    long *frequencies = calloc(256, sizeof(long));
    int leaf_count = 0;
    for (int i = 0; i < 256; i++) {
        if (frequencies[i] != 0) leaf_count++;
    }
    Node **nodes = malloc((2 * leaf_count - 1) * sizeof(Node*));
    int j = 0;
    for (int i = 0; i < 256; i++) {
        if (frequencies[i] != 0) {
            Node *leaf = malloc(sizeof(Node));
            *leaf = construct_leaf(frequencies[i], (char) i);
            nodes[j] = leaf;
            j++;
        }
    }
    sort_nodes(nodes, leaf_count);
    Node *root_node = construct_tree(nodes, leaf_count);

    return 0;
}
