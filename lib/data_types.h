#ifndef DATA_TYPES_H
#define DATA_TYPES_H

/*
 * A magic az a tomoritett fajlban szereplo azonosito.
 * A fajl beolvasasanal ezt ellenorizzuk.
 */
static const char magic[4] = {'H', 'U', 'F', 'F'};


// Jelzi, hogy egy node level (adatot tartalmaz) vagy csomopont.
typedef enum {
    LEAF,
    BRANCH
} node_type;


// A Huffman fa egy pontja: unionban tarolja a karaktert vagy a ket gyermek indexet.
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

/*
 * A tomoritett fajl minden fontos adatat tartalmazza: az azonosito szam, fajlnevek, fa, tomoritett adat es meretek.
 * A compress es decompress, valamint a read es write_compressed funkciok ezt a strukturat ertelmezik.
 */
typedef struct {
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
