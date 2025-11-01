#ifndef COMPRESS_H
#define COMPRESS_H

#include "data_types.h"

int count_frequencies(char *data, long data_len, long *frequencies);
Node* construct_tree(Node *nodes, long leaf_count);
Node construct_leaf(long frequency, char data);
Node construct_branch(Node *nodes, int left_index, int right_index);
void sort_nodes(Node *nodes, int len);

int compress(char *data, Node *root_node);
int decompress(char *data, Node *root_node);

#endif
