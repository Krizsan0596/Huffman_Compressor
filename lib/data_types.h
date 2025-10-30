#ifndef DATA_TYPES_H
#define DATA_TYPES_H

typedef struct{
    char *file_name;
    long original_size;
    char *original_file;
    char *huffman_tree;
    long tree_size; 
    char *compressed_data;
    long data_size; // In bits.
} compressed_file;

typedef enum {
    LEAF,
    BRANCH
} node_type;

typedef struct Node {
    node_type type;
    union {
        char data;
        struct {
            struct Node *left;
            struct Node *right;
        };
    };
} Node;

#endif
