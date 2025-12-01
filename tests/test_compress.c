#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "../lib/file.h"
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
    assert(compress((char *)input, len, nodes, root, cache, &compressed) == 0);
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
    int rc = compress("", 0, dummy_nodes, &dummy_nodes[0], cache, &compressed);
    assert(rc == 0);
    assert(compressed.data_size == 0);
    assert(compressed.compressed_data == NULL);
    (void)rc;

    free_cache(cache);
}

/* ===== Tests for run_compression function ===== */

static void test_run_compression_basic_file(void) {
    // Create a test file
    const char *test_file = "/tmp/test_run_compression_input.txt";
    const char *output_file = "/tmp/test_run_compression_output.huff";
    const char *test_content = "AAAABBBBCCCC";
    
    FILE *f = fopen(test_file, "w");
    assert(f != NULL);
    fprintf(f, "%s", test_content);
    fclose(f);
    
    // Remove output file if it exists
    unlink(output_file);
    
    Arguments args = {0};
    args.compress_mode = true;
    args.extract_mode = false;
    args.force = false;
    args.directory = false;
    args.input_file = (char *)test_file;
    args.output_file = (char *)output_file;
    
    int result = run_compression(args);
    assert(result == 0);
    
    // Verify output file was created
    struct stat st;
    assert(stat(output_file, &st) == 0);
    assert(st.st_size > 0);
    
    // Cleanup
    unlink(test_file);
    unlink(output_file);
}

static void test_run_compression_auto_output_filename(void) {
    // Create a test file
    const char *test_file = "/tmp/test_auto_output.txt";
    const char *expected_output = "/tmp/test_auto_output.huff";
    const char *test_content = "Hello World!";
    
    FILE *f = fopen(test_file, "w");
    assert(f != NULL);
    fprintf(f, "%s", test_content);
    fclose(f);
    
    // Remove expected output file if it exists
    unlink(expected_output);
    
    Arguments args = {0};
    args.compress_mode = true;
    args.extract_mode = false;
    args.force = false;
    args.directory = false;
    args.input_file = (char *)test_file;
    args.output_file = NULL;  // Should auto-generate
    
    int result = run_compression(args);
    assert(result == 0);
    
    // Verify output file was created with expected name
    struct stat st;
    assert(stat(expected_output, &st) == 0);
    assert(st.st_size > 0);
    
    // Cleanup
    unlink(test_file);
    unlink(expected_output);
}

static void test_run_compression_nonexistent_file(void) {
    Arguments args = {0};
    args.compress_mode = true;
    args.extract_mode = false;
    args.force = false;
    args.directory = false;
    args.input_file = "/tmp/nonexistent_file_12345.txt";
    args.output_file = "/tmp/test_output.huff";
    
    int result = run_compression(args);
    // Should return an error for non-existent file
    assert(result < 0);
}

static void test_run_compression_directory(void) {
    // Create test directory structure
    const char *test_dir = "/tmp/test_run_comp_dir";
    const char *output_file = "/tmp/test_run_comp_dir.huff";
    char subdir_path[256];
    char file1_path[256];
    char file2_path[256];
    
    snprintf(subdir_path, sizeof(subdir_path), "%s/subdir", test_dir);
    snprintf(file1_path, sizeof(file1_path), "%s/file1.txt", test_dir);
    snprintf(file2_path, sizeof(file2_path), "%s/subdir/file2.txt", test_dir);
    
    // Remove any existing directory
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
    system(cmd);
    unlink(output_file);
    
    // Create directories
    mkdir(test_dir, 0755);
    mkdir(subdir_path, 0755);
    
    // Create files
    FILE *f1 = fopen(file1_path, "w");
    assert(f1 != NULL);
    fprintf(f1, "Content of file 1");
    fclose(f1);
    
    FILE *f2 = fopen(file2_path, "w");
    assert(f2 != NULL);
    fprintf(f2, "Content of file 2");
    fclose(f2);
    
    Arguments args = {0};
    args.compress_mode = true;
    args.extract_mode = false;
    args.force = false;
    args.directory = true;
    args.input_file = (char *)test_dir;
    args.output_file = (char *)output_file;
    
    int result = run_compression(args);
    assert(result == 0);
    
    // Verify output file was created
    struct stat st;
    assert(stat(output_file, &st) == 0);
    assert(st.st_size > 0);
    
    // Cleanup
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
    system(cmd);
    unlink(output_file);
}

static void test_run_compression_force_overwrite(void) {
    // Create test file
    const char *test_file = "/tmp/test_force_input.txt";
    const char *output_file = "/tmp/test_force_output.huff";
    const char *test_content = "Test content for force overwrite";
    
    // Remove any existing files first
    unlink(test_file);
    unlink(output_file);
    
    FILE *f = fopen(test_file, "w");
    assert(f != NULL);
    fprintf(f, "%s", test_content);
    fclose(f);
    
    // Create an existing output file
    FILE *existing = fopen(output_file, "w");
    assert(existing != NULL);
    fprintf(existing, "Existing content");
    fclose(existing);
    
    // Get original file size
    struct stat original_stat;
    assert(stat(output_file, &original_stat) == 0);
    
    // Test with force=true - should overwrite successfully
    Arguments args = {0};
    args.compress_mode = true;
    args.extract_mode = false;
    args.force = true;  // Use force to avoid interactive prompt
    args.directory = false;
    args.input_file = (char *)test_file;
    args.output_file = (char *)output_file;
    
    int result = run_compression(args);
    assert(result == 0);
    
    // Verify file was overwritten (size should be different)
    struct stat new_stat;
    assert(stat(output_file, &new_stat) == 0);
    assert(new_stat.st_size != original_stat.st_size);
    
    // Cleanup
    unlink(test_file);
    unlink(output_file);
}

static void test_run_compression_empty_file(void) {
    // Create an empty test file
    const char *test_file = "/tmp/test_empty_file.txt";
    const char *output_file = "/tmp/test_empty_output.huff";
    
    FILE *f = fopen(test_file, "w");
    assert(f != NULL);
    fclose(f);
    
    unlink(output_file);
    
    Arguments args = {0};
    args.compress_mode = true;
    args.extract_mode = false;
    args.force = false;
    args.directory = false;
    args.input_file = (char *)test_file;
    args.output_file = (char *)output_file;
    
    int result = run_compression(args);
    // Empty file should return EMPTY_FILE error
    assert(result == EMPTY_FILE);
    
    // Cleanup
    unlink(test_file);
    unlink(output_file);
}

int main(void) {
    test_compress_basic_pattern();
    test_compress_zero_length();
    
    // run_compression tests
    test_run_compression_basic_file();
    test_run_compression_auto_output_filename();
    test_run_compression_nonexistent_file();
    test_run_compression_directory();
    test_run_compression_force_overwrite();
    test_run_compression_empty_file();
    
    return 0;
}
