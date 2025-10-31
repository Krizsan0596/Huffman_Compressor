#ifndef COMPRESS_H
#define COMPRESS_H

#include "data_types.h"
int count_frequencies(char *data, long data_len, long *frequencies);
Node* construct_tree(Node **nodes, long nodes_len);
int find_leaf(char *leaf, Node *root_node);
Node construct_leaf(long frequency, char data);
Node construct_branch(Node *left, Node *right);
void sort_nodes(Node **nodes, int count);

int compress(char *data, Node *root_node);
int decompress(char *data, Node *root_node);

#endif
