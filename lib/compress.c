#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <stdbool.h>
#include "file.h"
#include "compress.h"
#include "data_types.h"
#include "debugmalloc.h"

// Segedfuggveny a qsort rendezeshez
static int compare_nodes(const void *a, const void *b) {
    long freq_a = ((Node*)a)->frequency;
    long freq_b = ((Node*)b)->frequency;
    if (freq_a < freq_b) return -1;
    if (freq_a > freq_b) return 1;
    return 0;
}

 // A nodes tombot gyakorisag alapjan rendezi, hogy a Huffman fa felepitese konnyebb legyen.
void sort_nodes(Node *nodes, int len) {
    qsort(nodes, len, sizeof(Node), compare_nodes);
}

/*
 * Vegigmegy a nyers adaton, es helyben noveli a 256 elemu frekvenciatomb ertekeit.
 * 0-val ter vissza, miutan minden bajtot feldolgozott. A hivotol kapott frequencies tomb nullazott kell legyen.
 */
int count_frequencies(char *data, long data_len, long *frequencies) {
    for (int i = 0; i < data_len; i++){
        frequencies[(unsigned char) data[i]] += 1;
    }
    return 0;
}

char* generate_output_file(char *input_file){
    char *dir_end = strrchr(input_file, '/');
    char *name_end;
    if (dir_end != NULL) name_end = strrchr(dir_end, '.');
    else name_end = strrchr(input_file, '.');

    char *out;
    if (name_end != NULL) {
        int name_len = name_end - input_file;
        out = malloc(name_len + 6);
        if (out == NULL) {
            return NULL;
        }
        strncpy(out, input_file, name_len);
        out[name_len] = '\0';
        strcat(out, ".huff");
    }
    else {
        out = malloc(strlen(input_file) + 6);
        if (out == NULL) {
            return NULL;
        }
        strcpy(out, input_file);

        strcat(out, ".huff");
    }
    return out;
}

// Letrehoz egy levelet, amelyben a bajt es a hozza tartozo gyakorisag tarolodik.
Node construct_leaf(long frequency, char data) {
    Node leaf = {0};
    leaf.type = LEAF;
    leaf.frequency = frequency;
    leaf.data = data;
    return leaf;
}

/*
 * Osszeallit egy csomopontot, amely a tombben tarolt 2 gyerekenek az indexet tarolja.
 * A gyerekek gyakorisaganak osszege lesz ennek a gyakorisaga a Huffman algoritmus szerint.
 */
Node construct_branch(Node *nodes, int left_index, int right_index) {
    Node branch = {0};
    branch.type = BRANCH;
    branch.left = left_index;
    branch.right = right_index;
    branch.frequency = nodes[left_index].frequency + nodes[right_index].frequency;
    return branch;
}

/*
 * Osszevonja a rendezett leveleket, es Huffman fat epit beloluk.
 * A gyokerre mutato pointert adja vissza, vagy NULL-t, ha nincs egyetlen level sem.
 *
 * A keszitett csomopontokat a levelek utan rakja sorrendbe, igy mindig rendezett lesz a lista. 
 */
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

/*
 * Megnezi, hogy a keresett bajt helye a Huffman faban mar megtalalhato-e a cache tombben.
 * Ha van talalat, visszaadja a karakterlancot, kulonben NULL-t ad vissza.
 */
char* check_cache(char leaf, char **cache) {
    if (cache[(unsigned char) leaf] != NULL) return cache[(unsigned char) leaf];
    else return NULL;
}

/*
 * Rekurzivan bejarja a Huffman fat, megkeresi a bajt helyet es osszerakja az utvonalat.
 */
char* find_leaf(char leaf, Node *nodes, Node *root_node) {
    char *path = NULL; 
    if (root_node->type == LEAF) {
        if (root_node->data == leaf) {
            path = malloc(1);
            if (path != NULL) {
                path[0] = '\0';
            }
        }
        return path;
    }
    else {
        char *res = find_leaf(leaf, nodes, &nodes[root_node->left]);
        if (res != NULL) {
            path = malloc((strlen(res) + 2) * sizeof(char));
            if (path != NULL) {
                strcpy(path, "0");
                strcat(path, res);
            }
            free(res);
            return path;
        }
        res = find_leaf(leaf, nodes, &nodes[root_node->right]);
        if (res != NULL) {
            path = malloc((strlen(res) + 2) * sizeof(char));
            if (path != NULL) {
                strcpy(path, "1");
                strcat(path, res);
            }
            free(res);
            return path;
        }
        return NULL;
    }
}


/*
 * Vegigjarja a Huffman fat, es a kapott adatot tomoritett bitfolyamabba kodolja.
 * A kitomoriteshez szukseges adatokat betolti egy Compressed_file strukturaba.
 * 0-t ad vissza siker eseten, negativ ertekeket memoriafoglalasi vagy fa-bejarasi hiba eseten.
 */
int compress(char *original_data, long data_len, Node *nodes, Node *root_node, char** cache, Compressed_file *compressed_file) {
    if (data_len == 0) {
        compressed_file->data_size = 0;
        compressed_file->compressed_data = NULL;
        return 0;
    }

    compressed_file->compressed_data = malloc(data_len * sizeof(char));
    if (compressed_file->compressed_data == NULL) {
        compressed_file->data_size = 0;
        return MALLOC_ERROR;
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
                compressed_file->compressed_data = NULL;
                compressed_file->data_size = 0;
                return TREE_ERROR;
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
                buffer = 0;
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

