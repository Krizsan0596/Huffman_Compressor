#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "../lib/file.h"
#include "../lib/data_types.h"
#include "../lib/debugmalloc.h"

void test_file_io() {
    // 1. Setup
    Compressed_file original_data;
    memcpy(original_data.magic, magic, sizeof(original_data.magic));
    original_data.is_dir = false;
    original_data.original_size = 123;
    original_data.original_file = strdup("test.txt");
    original_data.tree_size = 45;
    original_data.huffman_tree = malloc(original_data.tree_size);
    memset(original_data.huffman_tree, 'A', original_data.tree_size);
    original_data.data_size = 80;
    long compressed_bytes = (long)ceil((double)original_data.data_size / 8.0);
    original_data.compressed_data = malloc(compressed_bytes);
    memset(original_data.compressed_data, 'B', compressed_bytes);
    original_data.file_name = strdup("test.huf");

    // 2. Action
    int write_result = write_compressed(&original_data, true);
    assert(write_result >= 0);

    // 3. Verification
    Compressed_file read_data;
    int read_result = read_compressed("test.huf", &read_data);
    assert(read_result == 0);

    assert(original_data.original_size == read_data.original_size);
    assert(strcmp(original_data.original_file, read_data.original_file) == 0);
    assert(original_data.tree_size == read_data.tree_size);
    assert(memcmp(original_data.huffman_tree, read_data.huffman_tree, original_data.tree_size) == 0);
    assert(original_data.data_size == read_data.data_size);
    assert(memcmp(original_data.compressed_data, read_data.compressed_data, compressed_bytes) == 0);

    // 4. Teardown
    free(original_data.original_file);
    free(original_data.huffman_tree);
    free(original_data.compressed_data);
    free(original_data.file_name);
    free(read_data.original_file);
    free(read_data.huffman_tree);
    free(read_data.compressed_data);
    free(read_data.file_name);

    remove("test.huf");

    printf("test_file_io passed.\n");
}

int main() {
    test_file_io();
    return 0;
}
