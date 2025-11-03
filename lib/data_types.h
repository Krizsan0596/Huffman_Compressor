#ifndef DATA_TYPES_H
#define DATA_TYPES_H

static const char magic[4] = {'H', 'U', 'F', 'F'};

typedef enum {
    LEAF,
    BRANCH
} node_type;

typedef struct Node {
    node_type type;
    long frequency;
    union {
        char data;
        struct {
            int left;
            int right;
        };
    };
} Node;

typedef struct{
    char magic[4];
    char *file_name;
    long original_size;
    char *original_file;
    Node *huffman_tree;
    long tree_size; 
    char *compressed_data;
    long data_size; // In bits.
} Compressed_file;

#endif
