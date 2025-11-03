#include <assert.h>
#include <math.h>
#include <string.h>
#include "../lib/compress.h"
#include "../lib/data_types.h"
#include "../lib/debugmalloc.h"

static void free_cache(char **cache) {
    for (int i = 0; i < 256; ++i) {
        if (cache[i] != NULL) {
            free(cache[i]);
        }
    }
    free(cache);
}

static void build_huffman_tree(const char *input, long len, Node **out_nodes, Node **out_root) {
    long *frequencies = calloc(256, sizeof(long));
    assert(frequencies != NULL);

    count_frequencies((char *)input, len, frequencies);

    int leaf_count = 0;
    for (int i = 0; i < 256; ++i) {
        if (frequencies[i] != 0) {
            leaf_count++;
        }
    }

    assert(leaf_count > 0);

    Node *nodes = malloc((2 * leaf_count - 1) * sizeof(Node));
    assert(nodes != NULL);

    int idx = 0;
    for (int i = 0; i < 256; ++i) {
        if (frequencies[i] != 0) {
            nodes[idx++] = construct_leaf(frequencies[i], (char)i);
        }
    }

    free(frequencies);

    sort_nodes(nodes, leaf_count);
    Node *root = construct_tree(nodes, leaf_count);
    assert(root != NULL);

    *out_nodes = nodes;
    *out_root = root;
}

static void test_compress_basic_pattern(void) {
    const char *input = "AAAABB";
    const long len = (long)strlen(input);

    Node *nodes = NULL;
    Node *root = NULL;
    build_huffman_tree(input, len, &nodes, &root);

    char **cache = calloc(256, sizeof(char *));
    assert(cache != NULL);

    Compressed_file compressed = {0};
    int res = compress((char *)input, len, nodes, root, cache, &compressed);
    assert(res == 0);
    assert(compressed.data_size == 6);          // 6 characters encoded with 1 bit per symbol
    assert(compressed.compressed_data != NULL);
    assert(compressed.compressed_data[0] == (char)0xF0);

    free(compressed.compressed_data);
    free_cache(cache);
    free(nodes);
}

static void test_compress_zero_length(void) {
    Node dummy_nodes[1];
    dummy_nodes[0] = construct_leaf(1, 'A');

    char **cache = calloc(256, sizeof(char *));
    assert(cache != NULL);

    Compressed_file compressed = {0};
    int res = compress("", 0, dummy_nodes, &dummy_nodes[0], cache, &compressed);
    assert(res == 0);
    assert(compressed.data_size == 0);
    assert(compressed.compressed_data == NULL);

    free_cache(cache);
}

int main(void) {
    test_compress_basic_pattern();
    test_compress_zero_length();
    return 0;
}
