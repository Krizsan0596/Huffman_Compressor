#include <stdlib.h>
#include "compress.h"
#include "data_types.h"

static int compare_nodes(const void *a, const void *b) {
    Node node_a = *(Node*)a;
    Node node_b = *(Node*)b;
    return (node_a.frequency - node_b.frequency);
}

void sort_nodes(Node *nodes, int len) {
    qsort(nodes, len, sizeof(Node), compare_nodes);
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

Node construct_branch(Node *nodes, int left_index, int right_index) {
    Node branch;
    branch.type = BRANCH;
    branch.left = left_index;
    branch.right = right_index;
    branch.frequency = nodes[left_index].frequency + nodes[right_index].frequency;
    return branch;
}

Node* construct_tree(Node *nodes, long leaf_count) { // nodes is sorted
    if (leaf_count <= 0) return NULL;
    if (leaf_count == 1) {
        return &nodes[0];
    }
    long current_leaf = 0;
    long current_branch = leaf_count;
    long last_branch = leaf_count;

    for (int i = 0; i < leaf_count - 1; i++) {
        int left_index, right_index;

        if (current_leaf < leaf_count && (current_branch == last_branch || nodes[current_leaf].frequency <= nodes[current_branch].frequency)) {
            left_index = current_leaf++;
        } else {
            left_index = current_branch++;
        }

        if (current_leaf < leaf_count && (current_branch == last_branch || nodes[current_leaf].frequency <= nodes[current_branch].frequency)) {
            right_index = current_leaf++;
        } else {
            right_index = current_branch++;
        }

        nodes[last_branch] = construct_branch(nodes, left_index, right_index);
        last_branch++;
    }
    return &nodes[last_branch - 1];
}
