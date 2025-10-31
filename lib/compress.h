#ifndef COMPRESS_H
#define COMPRESS_H

#include "data_types.h"
int count_frequencies(char *data, long data_len, long *frequencies);
int construct_tree(long *frequencies, Node *root_node);
Node construct_leaf(long frequency, char data);
Node construct_branch(Node *left, Node *right);
int find_leaf(char *leaf, Node *root_node);

int compress(char *data, Node *root_node);
int decompress(char *data, Node *root_node);

#endif
