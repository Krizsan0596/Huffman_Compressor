#include <stdlib.h>
#include "compress.h"
#include "data_types.h"

static int compare_nodes(const void *a, const void *b) {
    Node *node_a = *(Node**)a;
    Node *node_b = *(Node**)b;
    return (node_a->frequency - node_b->frequency);
}

void sort_nodes(Node **nodes, int len) {
    qsort(nodes, len, sizeof(Node*), compare_nodes);
}

int len_frequencies(char *data, long data_len, long *frequencies) {
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

Node* construct_tree(Node **nodes, long leaf_count) { // nodes is sorted
    if (leaf_count <= 0) return NULL;
    if (leaf_count == 1) {
        return nodes[0];
    }
    long current_leaf = 0;
    long current_branch = leaf_count;
    long last_branch = leaf_count;
    Node *left, *right;
    while (current_leaf < leaf_count) {
        if (current_branch == last_branch || nodes[current_leaf]->frequency <= nodes[current_branch]->frequency) {  // branch is empty or leaf < branch
            left = nodes[current_branch];
            current_branch++;
        }
        else {
            left = nodes[current_leaf];
            current_leaf++;
        }
        if (current_branch == last_branch || nodes[current_leaf]->frequency <= nodes[current_branch]->frequency) {  // branch is empty or leaf < branch
            right = nodes[current_branch];
            current_branch++;
        }
        else {
            right = nodes[current_leaf];
            current_leaf++;
        }

        Node *branch = malloc(sizeof(Node));
        *branch = construct_branch(left, right);
        nodes[last_branch] = branch;
        last_branch++;
    }
    return nodes[last_branch - 1];
}
