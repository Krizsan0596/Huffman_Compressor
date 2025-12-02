#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../lib/util.h"
#include "../lib/huffman.h"
#include "../lib/file.h"
#include "../lib/data_types.h"
#include "../lib/debugmalloc.h"

int main() {
    char *data = "hello world";
    long data_len = strlen(data);

    long *frequencies = calloc(256, sizeof(long));
    count_frequencies(data, data_len, frequencies);

    int leaf_count = 0;
    for (int i = 0; i < 256; i++) {
        if (frequencies[i] != 0) {
            leaf_count++;
        }
    }

    Node *nodes = malloc((2 * leaf_count - 1) * sizeof(Node));
    int j = 0;
    for (int i = 0; i < 256; i++) {
        if (frequencies[i] != 0) {
            nodes[j] = construct_leaf(frequencies[i], (char) i);
            j++;
        }
    }
    free(frequencies);

    sort_nodes(nodes, leaf_count);
    Node *root_node = construct_tree(nodes, leaf_count);

    long tree_size;
    if (root_node != NULL) {
        tree_size = (root_node - nodes) + 1;
    }
    else {
        free(nodes);
        return TREE_ERROR;
    }

    char **cache = calloc(256, sizeof(char*));

    Compressed_file *compressed_file = malloc(sizeof(Compressed_file));
    int compress_result = compress(data, data_len, nodes, root_node, cache, compressed_file);
    if (compress_result != 0) {
        printf("Compression failed with error code %d!\n", compress_result);
        free(nodes);
        for(int i=0; i<256; ++i) {
            if (cache[i] != NULL) {
                free(cache[i]);
            }
        }
        free(cache);
        free(compressed_file);
        return 3;
    }

    compressed_file->huffman_tree = nodes;
    compressed_file->tree_size = tree_size * sizeof(Node);

    char *raw_data = malloc(data_len * sizeof(char));
    int decompress_result = decompress(compressed_file, raw_data);
    if (decompress_result != 0) {
        printf("Decompression failed with error code %d!\n", decompress_result);
        free(nodes);
        for(int i=0; i<256; ++i) {
            if (cache[i] != NULL) {
                free(cache[i]);
            }
        }
        free(cache);
        free(compressed_file->compressed_data);
        free(compressed_file);
        free(raw_data);
        return 4;
    }

    if (memcmp(data, raw_data, data_len) != 0) {
        printf("Decompression failed!\n");
        printf("Original:  %s\n", data);
        printf("Decompressed: %s\n", raw_data);
    } else {
        printf("Decompression successful!\n");
    }

    free(nodes);
    for(int i=0; i<256; ++i) {
        if (cache[i] != NULL) {
            free(cache[i]);
        }
    }
    free(cache);
    free(compressed_file->compressed_data);
    free(compressed_file);
    free(raw_data);

    // ==========================================
    // Test run_decompression function
    // ==========================================
    printf("Testing run_decompression function...\n");
    
    // Test 1: Basic run_decompression with file
    printf("  Test 1: Basic run_decompression with file...\n");
    {
        // Create a test file
        char *test_content = "This is a test file for run_decompression testing.";
        char *test_input = "test_decomp_input.txt";
        char *test_compressed = "test_decomp_input.huff";
        char *test_output = "test_decomp_output.txt";
        
        // Write test input file
        int write_res = write_raw(test_input, test_content, strlen(test_content), true);
        if (write_res < 0) {
            fprintf(stderr, "Error: Failed to write test input file\n");
            return 1;
        }
        
        // Use run_compression to create a compressed file
        Arguments compress_args = {0};
        compress_args.compress_mode = true;
        compress_args.extract_mode = false;
        compress_args.force = true;
        compress_args.directory = false;
        compress_args.input_file = test_input;
        compress_args.output_file = NULL;  // Use default output
        
        int comp_result = run_compression(compress_args);
        if (comp_result != 0) {
            fprintf(stderr, "Error: run_compression failed, code: %d\n", comp_result);
            remove(test_input);
            return 1;
        }
        
        // Now test run_decompression
        Arguments decomp_args = {0};
        decomp_args.compress_mode = false;
        decomp_args.extract_mode = true;
        decomp_args.force = true;
        decomp_args.directory = false;
        decomp_args.input_file = test_compressed;
        decomp_args.output_file = test_output;
        
        int result = run_decompression(decomp_args);
        if (result != 0) {
            fprintf(stderr, "Error: run_decompression failed, code: %d\n", result);
            remove(test_input);
            remove(test_compressed);
            return 1;
        }
        
        // Verify the decompressed content
        char *decompressed_content = NULL;
        int read_size = read_raw(test_output, &decompressed_content);
        if (read_size < 0) {
            fprintf(stderr, "Error: Failed to read decompressed output file\n");
            remove(test_input);
            remove(test_compressed);
            remove(test_output);
            return 1;
        }
        
        if ((long)read_size != (long)strlen(test_content) || 
            memcmp(test_content, decompressed_content, strlen(test_content)) != 0) {
            fprintf(stderr, "Error: Decompressed content does not match original\n");
            free(decompressed_content);
            remove(test_input);
            remove(test_compressed);
            remove(test_output);
            return 1;
        }
        
        printf("    Basic run_decompression test passed.\n");
        free(decompressed_content);
        
        // Cleanup
        remove(test_input);
        remove(test_compressed);
        remove(test_output);
    }
    
    // Test 2: run_decompression with default output (use original filename)
    printf("  Test 2: run_decompression with default output...\n");
    {
        char *test_content = "Test content for default output test.";
        char *test_input = "test_default_input.txt";
        char *test_compressed = "test_default_input.huff";
        
        // Write test input file
        int write_res = write_raw(test_input, test_content, strlen(test_content), true);
        if (write_res < 0) {
            fprintf(stderr, "Error: Failed to write test input file\n");
            return 1;
        }
        
        // Compress the file
        Arguments compress_args = {0};
        compress_args.compress_mode = true;
        compress_args.extract_mode = false;
        compress_args.force = true;
        compress_args.directory = false;
        compress_args.input_file = test_input;
        compress_args.output_file = NULL;
        
        int comp_result = run_compression(compress_args);
        if (comp_result != 0) {
            fprintf(stderr, "Error: run_compression failed, code: %d\n", comp_result);
            remove(test_input);
            return 1;
        }
        
        // Remove the original file so decompression recreates it
        remove(test_input);
        
        // Decompress with NULL output (should restore to original filename)
        Arguments decomp_args = {0};
        decomp_args.compress_mode = false;
        decomp_args.extract_mode = true;
        decomp_args.force = true;
        decomp_args.directory = false;
        decomp_args.input_file = test_compressed;
        decomp_args.output_file = NULL;  // Use stored original filename
        
        int result = run_decompression(decomp_args);
        if (result != 0) {
            fprintf(stderr, "Error: run_decompression with default output failed, code: %d\n", result);
            remove(test_compressed);
            return 1;
        }
        
        // Check that the original file was recreated
        struct stat st;
        if (stat(test_input, &st) != 0) {
            fprintf(stderr, "Error: Original file was not recreated\n");
            remove(test_compressed);
            return 1;
        }
        
        // Verify content
        char *decompressed_content = NULL;
        int read_size = read_raw(test_input, &decompressed_content);
        if (read_size < 0 || (long)read_size != (long)strlen(test_content) ||
            memcmp(test_content, decompressed_content, strlen(test_content)) != 0) {
            fprintf(stderr, "Error: Decompressed content does not match original\n");
            if (decompressed_content != NULL) free(decompressed_content);
            remove(test_input);
            remove(test_compressed);
            return 1;
        }
        
        printf("    run_decompression with default output test passed.\n");
        free(decompressed_content);
        
        // Cleanup
        remove(test_input);
        remove(test_compressed);
    }
    
    // Test 3: run_decompression error handling (non-existent file)
    printf("  Test 3: run_decompression error handling...\n");
    {
        Arguments decomp_args = {0};
        decomp_args.compress_mode = false;
        decomp_args.extract_mode = true;
        decomp_args.force = true;
        decomp_args.directory = false;
        decomp_args.input_file = "non_existent_file_12345.huff";
        decomp_args.output_file = "output.txt";
        
        int result = run_decompression(decomp_args);
        if (result == 0) {
            fprintf(stderr, "Error: run_decompression should fail for non-existent file\n");
            return 1;
        }
        
        printf("    run_decompression error handling test passed. Error code: %d\n", result);
    }
    
    printf("All run_decompression tests passed!\n");

    return 0;
}
