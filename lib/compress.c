#include "compress.h"
#include "data_types.h"
#include <stdlib.h>
static int comp(const void *a, const void *b) {
    return (*(long*)a - *(long*)b);
}

int count_frequencies(char *data, long data_len, long *frequencies) {
    for (int i = 0; i < data_len; i++){
        frequencies[(unsigned char) data[i]] += 1;
    }
    return 0;
}

Node construct_leaf(long frequency, char data) {
    Node leaf;
    leaf.type = LEAF;
    leaf.frequency = frequency;
    leaf.data = data;
    return leaf;
}

Node construct_branch(Node *left, Node *right) {
    Node branch;
    branch.type = BRANCH;
    branch.left = left;
    branch.right = right;
    branch.frequency = left->frequency + right->frequency;
    return branch;
}

int construct_tree(long *frequencies, Node *root_node);
