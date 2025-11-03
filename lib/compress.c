#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include "compress.h"
#include "data_types.h"

static int compare_nodes(const void *a, const void *b) {
    long freq_a = ((Node*)a)->frequency;
    long freq_b = ((Node*)b)->frequency;
    if (freq_a < freq_b) return -1;
    if (freq_a > freq_b) return 1;
    return 0;
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

char* check_cache(char leaf, char **cache) {
    if (cache[(unsigned char) leaf] != NULL) return cache[(unsigned char) leaf];
    else return NULL;
}

char* find_leaf(char leaf, Node *nodes, Node *root_node) {
    char *path = NULL; 
    if (root_node->type == LEAF) {
        if (root_node->data == leaf) {
            path = malloc(1);
            path[0] = '\0';
        }
        return path;
    }
    else {
        char *res = find_leaf(leaf, nodes, &nodes[root_node->left]);
        if (res != NULL) {
            path = malloc((strlen(res) + 2) * sizeof(char));
            strcpy(path, "0");
            strcat(path, res);
            free(res);
            return path;
        }
        res = find_leaf(leaf, nodes, &nodes[root_node->right]);
        if (res != NULL) {
            path = malloc((strlen(res) + 2) * sizeof(char));
            strcpy(path, "1");
            strcat(path, res);
            free(res);
        }
        return path;
    }
}


int compress(char *original_data, long data_len, Node *nodes, Node *root_node, char** cache, Compressed_file *compressed_file) {
    if (data_len == 0) {
        compressed_file->data_size = 0;
        compressed_file->compressed_data = NULL;
        return 0;
    }

    compressed_file->compressed_data = malloc(data_len * sizeof(char));
    if (compressed_file->compressed_data == NULL) {
        return -1; // Malloc error
    }

    long total_bits = 0;
    unsigned char buffer = 0;
    int bit_count = 0;

    for (long i = 0; i < data_len; i++) {
        char *path = check_cache(original_data[i], cache);
        if (path == NULL) {
            path = find_leaf(original_data[i], nodes, root_node);
            if (path != NULL) {
                cache[(unsigned char)original_data[i]] = path;
            } else {
                free(compressed_file->compressed_data);
                return -2; // tree error
            }
        }

        for (int j = 0; path[j] != '\0'; j++) {
            if (path[j] == '1') {
                buffer |= (1 << (7 - bit_count));
            }
            bit_count++;
            if (bit_count == 8) {
                compressed_file->compressed_data[total_bits / 8] = buffer;
                total_bits += 8;
                buffer &= 0;
                bit_count = 0;
            }
        }
    }

    if (bit_count > 0) {
        compressed_file->compressed_data[total_bits / 8] = buffer;
        total_bits += bit_count;
    }

    compressed_file->data_size = total_bits;

    char *temp = realloc(compressed_file->compressed_data, ceil((double)total_bits / 8.0));
    if (temp != NULL) {
        compressed_file->compressed_data = temp;
    }

    return 0;
}

