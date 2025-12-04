#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <assert.h>
#include "../lib/directory.h"
#include "../lib/data_types.h"
#include "../lib/debugmalloc.h"

// Helper function to recursively delete a directory
static int remove_directory_recursive(const char *path) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
        return unlink(path);
    }

    int result = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                int ret = remove_directory_recursive(full_path);
                if (ret != 0) result = ret;
            } else {
                int ret = unlink(full_path);
                if (ret != 0) result = ret;
            }
        } else {
            // Attempt to remove anyway (could be a broken symlink, etc.)
            int ret = unlink(full_path);
            if (ret != 0) result = ret;
        }
    }
    closedir(dir);
    int ret = rmdir(path);
    return (result != 0) ? result : ret;
}

// Helper function to free Directory_item arrays
static void free_directory_items(Directory_item *items, int count) {
    if (items == NULL) {
        return;
    }
    for (int i = 0; i < count; i++) {
        if (items[i].is_dir) {
            free(items[i].dir_path);
        } else {
            free(items[i].file_path);
            free(items[i].file_data);
        }
    }
    free(items);
}

static long serialize_archive_to_buffer(char *path, char **buffer, int *archive_size, long *dir_size_out) {
    *archive_size = 0;

    FILE *stream = tmpfile();
    if (stream == NULL) return FILE_WRITE_ERROR;

    if (fwrite(archive_size, sizeof(int), 1, stream) != 1) {
        fclose(stream);
        return FILE_WRITE_ERROR;
    }

    long dir_size = archive_directory(path, stream, archive_size);
    if (dir_size < 0) {
        fclose(stream);
        return dir_size;
    }

    long file_size = ftell(stream);
    if (file_size < 0) {
        fclose(stream);
        return FILE_WRITE_ERROR;
    }

    if (fseek(stream, 0, SEEK_SET) != 0 || fwrite(archive_size, sizeof(int), 1, stream) != 1 || fseek(stream, 0, SEEK_SET) != 0) {
        fclose(stream);
        return FILE_WRITE_ERROR;
    }

    *buffer = malloc(file_size);
    if (*buffer == NULL) {
        fclose(stream);
        return MALLOC_ERROR;
    }

    size_t read_bytes = fread(*buffer, 1, file_size, stream);
    fclose(stream);
    if (read_bytes != (size_t)file_size) {
        free(*buffer);
        *buffer = NULL;
        return FILE_WRITE_ERROR;
    }

    if (dir_size_out != NULL) {
        *dir_size_out = dir_size;
    }

    return file_size;
}

// Function to recursively compare two directories
int compare_directories(const char *path1, const char *path2) {
    DIR *dir1 = opendir(path1);
    if (dir1 == NULL) {
        perror("opendir path1");
        return -1;
    }

    DIR *dir2 = opendir(path2);
    if (dir2 == NULL) {
        perror("opendir path2");
        closedir(dir1);
        return -1;
    }

    struct dirent *entry1;
    while ((entry1 = readdir(dir1)) != NULL) {
        if (strcmp(entry1->d_name, ".") == 0 || strcmp(entry1->d_name, "..") == 0) {
            continue;
        }

        char full_path1[1024];
        snprintf(full_path1, sizeof(full_path1), "%s/%s", path1, entry1->d_name);

        char full_path2[1024];
        snprintf(full_path2, sizeof(full_path2), "%s/%s", path2, entry1->d_name);

        struct stat stat1;
        if (stat(full_path1, &stat1) != 0) {
            perror("stat path1");
            closedir(dir1);
            closedir(dir2);
            return -1;
        }

        struct stat stat2;
        if (stat(full_path2, &stat2) != 0) {
            perror("stat path2");
            closedir(dir1);
            closedir(dir2);
            return -1;
        }

        if (S_ISDIR(stat1.st_mode) != S_ISDIR(stat2.st_mode)) {
            fprintf(stderr, "Type mismatch for %s\n", entry1->d_name);
            closedir(dir1);
            closedir(dir2);
            return -1;
        }

        if (S_ISDIR(stat1.st_mode)) {
            if (compare_directories(full_path1, full_path2) != 0) {
                closedir(dir1);
                closedir(dir2);
                return -1;
            }
        } else {
            if (stat1.st_size != stat2.st_size) {
                fprintf(stderr, "Size mismatch for %s\n", entry1->d_name);
                closedir(dir1);
                closedir(dir2);
                return -1;
            }

            FILE *f1 = fopen(full_path1, "rb");
            if (f1 == NULL) {
                perror("fopen path1");
                closedir(dir1);
                closedir(dir2);
                return -1;
            }

            FILE *f2 = fopen(full_path2, "rb");
            if (f2 == NULL) {
                perror("fopen path2");
                fclose(f1);
                closedir(dir1);
                closedir(dir2);
                return -1;
            }

            char buf1[1024];
            char buf2[1024];
            size_t bytes_read1;
            while ((bytes_read1 = fread(buf1, 1, sizeof(buf1), f1)) > 0) {
                size_t bytes_read2 = fread(buf2, 1, sizeof(buf2), f2);
                if (bytes_read1 != bytes_read2 || memcmp(buf1, buf2, bytes_read1) != 0) {
                    fprintf(stderr, "Content mismatch for %s\n", entry1->d_name);
                    fclose(f1);
                    fclose(f2);
                    closedir(dir1);
                    closedir(dir2);
                    return -1;
                }
            }

            fclose(f1);
            fclose(f2);
        }
    }

    closedir(dir1);
    closedir(dir2);
    return 0;
}


int main() {

    char *test_dir = "../tests/test_dir";
    char *output_dir = "output_dir";

    // Create test directory
    mkdir("../tests", 0755);
    mkdir("../tests/test_dir", 0755);
    mkdir("../tests/test_dir/subdir", 0755);
    FILE *f1 = fopen("../tests/test_dir/file1.txt", "w");
    if (f1) {
        fprintf(f1, "This is file1.\n");
        fclose(f1);
    }
    FILE *f2 = fopen("../tests/test_dir/subdir/file2.txt", "w");
    if (f2) {
        fprintf(f2, "This is file2.\n");
        fclose(f2);
    }

    // Create output directory
    remove_directory_recursive(output_dir);
    mkdir(output_dir, 0755);

    // 1. Archive the directory
    int archive_size = 0;
    long dir_size = 0;

    char original_cwd[1024];
    if (getcwd(original_cwd, sizeof(original_cwd)) == NULL) {
        perror("getcwd() error");
        return 1;
    }

    if (chdir(test_dir) != 0) {
        perror("chdir() error");
        return 1;
    }

    char *buffer = NULL;
    long buffer_size = serialize_archive_to_buffer(".", &buffer, &archive_size, &dir_size);

    if (chdir(original_cwd) != 0) {
        perror("chdir() error");
        return 1;
    }

    if (buffer_size < 0) {
        fprintf(stderr, "Error: Serialization failed with code %ld\n", buffer_size);
        return 1;
    }

    // 3. Deserialize the archive
    Directory_item *deserialized_archive = NULL;
    int deserialized_size = deserialize_archive(&deserialized_archive, buffer);
    if (deserialized_size < 0) {
        fprintf(stderr, "Error: Deserialization failed with code %d\n", deserialized_size);
        free(buffer);
        return 1;
    }

    // 4. Extract the directory
    if (chdir(output_dir) != 0) {
        perror("chdir() error");
        return 1;
    }
    if (extract_directory(".", deserialized_archive, deserialized_size, true, false) != 0) {
        if (chdir(original_cwd) != 0) {
            perror("chdir() error");
        }
        fprintf(stderr, "Error: Extraction failed\n");
        free(buffer);
        free(deserialized_archive);
        return 1;
    }
    if (chdir(original_cwd) != 0) {
        perror("chdir() error");
        return 1;
    }

    // 5. Compare the original and extracted directories
    if (compare_directories(test_dir, output_dir) != 0) {
        fprintf(stderr, "Error: Directories do not match!\n");
        // Cleanup
        free(buffer);
        free(deserialized_archive);
        return 1;
    }


    // 6. Cleanup
    free(buffer);
    for (int i = 0; i < deserialized_size; i++) {
        if (deserialized_archive[i].is_dir) {
            free(deserialized_archive[i].dir_path);
        }
    }
    for (int i = 0; i < deserialized_size; i++) {
        if (!deserialized_archive[i].is_dir) {
            free(deserialized_archive[i].file_path);
            free(deserialized_archive[i].file_data);
        }
    }
    free(deserialized_archive);

    remove_directory_recursive(output_dir);
    remove_directory_recursive(test_dir);

    // ==========================================
    // Test prepare_directory function
    // ==========================================
    printf("Testing prepare_directory function...\n");

    // Create test directory for prepare_directory tests
    char *prep_test_dir = "../tests/prep_test_dir";
    char *prep_output_dir = "prep_output_dir";
    
    mkdir("../tests", 0755);
    mkdir("../tests/prep_test_dir", 0755);
    mkdir("../tests/prep_test_dir/subdir", 0755);
    
    FILE *pf1 = fopen("../tests/prep_test_dir/file1.txt", "w");
    if (pf1) {
        fprintf(pf1, "This is file1 for prepare_directory test.\n");
        fclose(pf1);
    }
    FILE *pf2 = fopen("../tests/prep_test_dir/subdir/file2.txt", "w");
    if (pf2) {
        fprintf(pf2, "This is file2 for prepare_directory test.\n");
        fclose(pf2);
    }

    // Test 1: prepare_directory with relative path
    printf("  Test 1: prepare_directory with relative path...\n");
    {
        char *data = NULL;
        int directory_size = 0;
        int result = prepare_directory(prep_test_dir, &data, &directory_size);
        if (result < 0) {
            fprintf(stderr, "Error: prepare_directory failed with relative path, code: %d\n", result);
            return 1;
        }
        if (data == NULL) {
            fprintf(stderr, "Error: prepare_directory returned NULL data\n");
            return 1;
        }
        if (directory_size <= 0) {
            fprintf(stderr, "Error: prepare_directory returned invalid directory_size: %d\n", directory_size);
            free(data);
            return 1;
        }
        printf("    Relative path test passed. Directory size: %d bytes\n", directory_size);
        
        // Verify we can deserialize the archive
        Directory_item *prep_archive = NULL;
        int prep_archive_size = deserialize_archive(&prep_archive, data);
        if (prep_archive_size < 0) {
            fprintf(stderr, "Error: deserialize_archive failed after prepare_directory\n");
            free(data);
            return 1;
        }
        
        // Verify archive has expected items:
        // 1. prep_test_dir/ (root directory)
        // 2. prep_test_dir/subdir/ (subdirectory)
        // 3. prep_test_dir/file1.txt (file)
        // 4. prep_test_dir/subdir/file2.txt (file)
        // Total: 4 items
        if (prep_archive_size != 4) {
            fprintf(stderr, "Error: expected 4 items in archive, got %d\n", prep_archive_size);
            free(data);
            for (int i = 0; i < prep_archive_size; i++) {
                if (prep_archive[i].is_dir) {
                    free(prep_archive[i].dir_path);
                } else {
                    free(prep_archive[i].file_path);
                    free(prep_archive[i].file_data);
                }
            }
            free(prep_archive);
            return 1;
        }
        
        // Cleanup
        free(data);
        for (int i = 0; i < prep_archive_size; i++) {
            if (prep_archive[i].is_dir) {
                free(prep_archive[i].dir_path);
            } else {
                free(prep_archive[i].file_path);
                free(prep_archive[i].file_data);
            }
        }
        free(prep_archive);
    }
    
    // Test 2: prepare_directory with absolute path (includes full round-trip verification)
    printf("  Test 2: prepare_directory with absolute path...\n");
    {
        // Get absolute path to test directory
        char abs_path[1024];
        char saved_cwd[1024];
        if (getcwd(saved_cwd, sizeof(saved_cwd)) == NULL) {
            perror("getcwd error");
            return 1;
        }
        if (chdir(prep_test_dir) != 0) {
            perror("chdir error");
            return 1;
        }
        if (getcwd(abs_path, sizeof(abs_path)) == NULL) {
            perror("getcwd error");
            return 1;
        }
        if (chdir(saved_cwd) != 0) {
            perror("chdir error");
            return 1;
        }
        
        char *data = NULL;
        int directory_size = 0;
        int result = prepare_directory(abs_path, &data, &directory_size);
        if (result < 0) {
            fprintf(stderr, "Error: prepare_directory failed with absolute path, code: %d\n", result);
            return 1;
        }
        if (data == NULL) {
            fprintf(stderr, "Error: prepare_directory returned NULL data for absolute path\n");
            return 1;
        }
        if (directory_size <= 0) {
            fprintf(stderr, "Error: prepare_directory returned invalid directory_size for absolute path: %d\n", directory_size);
            free(data);
            return 1;
        }
        printf("    Absolute path test passed. Directory size: %d bytes\n", directory_size);
        
        // Verify we can deserialize
        Directory_item *prep_archive = NULL;
        int prep_archive_size = deserialize_archive(&prep_archive, data);
        if (prep_archive_size < 0) {
            fprintf(stderr, "Error: deserialize_archive failed after prepare_directory with absolute path\n");
            free(data);
            return 1;
        }
        
        // With absolute path, paths should be stored relative to the parent directory
        // So we can extract and verify the round-trip works
        remove_directory_recursive(prep_output_dir);
        mkdir(prep_output_dir, 0755);
        
        if (chdir(prep_output_dir) != 0) {
            perror("chdir error");
            free(data);
            for (int i = 0; i < prep_archive_size; i++) {
                if (prep_archive[i].is_dir) {
                    free(prep_archive[i].dir_path);
                } else {
                    free(prep_archive[i].file_path);
                    free(prep_archive[i].file_data);
                }
            }
            free(prep_archive);
            return 1;
        }
        
        if (extract_directory(".", prep_archive, prep_archive_size, true, false) != 0) {
            if (chdir(original_cwd) != 0) {
                perror("chdir error");
            }
            fprintf(stderr, "Error: extract_directory failed for absolute path archive\n");
            free(data);
            for (int i = 0; i < prep_archive_size; i++) {
                if (prep_archive[i].is_dir) {
                    free(prep_archive[i].dir_path);
                } else {
                    free(prep_archive[i].file_path);
                    free(prep_archive[i].file_data);
                }
            }
            free(prep_archive);
            return 1;
        }
        
        if (chdir(original_cwd) != 0) {
            perror("chdir error");
            return 1;
        }
        
        // Build path to compare - the extracted dir should be prep_output_dir/<dirname>
        // Extract directory name from prep_test_dir (last component after last '/')
        char *dirname = strrchr(prep_test_dir, '/');
        dirname = (dirname != NULL) ? dirname + 1 : prep_test_dir;
        char extracted_path[1024];
        snprintf(extracted_path, sizeof(extracted_path), "%s/%s", prep_output_dir, dirname);
        
        // Compare directories
        if (compare_directories(prep_test_dir, extracted_path) != 0) {
            fprintf(stderr, "Error: Directories do not match after prepare_directory with absolute path\n");
            free(data);
            for (int i = 0; i < prep_archive_size; i++) {
                if (prep_archive[i].is_dir) {
                    free(prep_archive[i].dir_path);
                } else {
                    free(prep_archive[i].file_path);
                    free(prep_archive[i].file_data);
                }
            }
            free(prep_archive);
            return 1;
        }
        printf("    Absolute path round-trip verification passed.\n");
        
        // Cleanup
        free(data);
        for (int i = 0; i < prep_archive_size; i++) {
            if (prep_archive[i].is_dir) {
                free(prep_archive[i].dir_path);
            } else {
                free(prep_archive[i].file_path);
                free(prep_archive[i].file_data);
            }
        }
        free(prep_archive);
        remove_directory_recursive(prep_output_dir);
    }
    
    // Test 3: prepare_directory with non-existent path (error handling)
    printf("  Test 3: prepare_directory with non-existent path...\n");
    {
        char *data = NULL;
        int directory_size = 0;
        int result = prepare_directory("./non_existent_directory_12345", &data, &directory_size);
        if (result >= 0) {
            fprintf(stderr, "Error: prepare_directory should fail for non-existent directory\n");
            if (data != NULL) free(data);
            return 1;
        }
        if (data != NULL) {
            fprintf(stderr, "Error: data should be NULL after failure\n");
            free(data);
            return 1;
        }
        printf("    Non-existent path error handling test passed. Error code: %d\n", result);
    }
    
    // Test 4: Verify working directory is restored after prepare_directory
    printf("  Test 4: Verify working directory is preserved...\n");
    {
        char cwd_before[1024];
        char cwd_after[1024];
        
        if (getcwd(cwd_before, sizeof(cwd_before)) == NULL) {
            perror("getcwd error");
            return 1;
        }
        
        char *data = NULL;
        int directory_size = 0;
        int result = prepare_directory(prep_test_dir, &data, &directory_size);
        
        if (getcwd(cwd_after, sizeof(cwd_after)) == NULL) {
            perror("getcwd error");
            if (data != NULL) free(data);
            return 1;
        }
        
        if (result < 0) {
            fprintf(stderr, "Error: prepare_directory failed, code: %d\n", result);
            return 1;
        }
        
        if (strcmp(cwd_before, cwd_after) != 0) {
            fprintf(stderr, "Error: Working directory changed after prepare_directory!\n");
            fprintf(stderr, "  Before: %s\n", cwd_before);
            fprintf(stderr, "  After:  %s\n", cwd_after);
            free(data);
            return 1;
        }
        printf("    Working directory preservation test passed.\n");
        free(data);
    }
    
    // Cleanup test directory
    remove_directory_recursive(prep_test_dir);
    
    printf("All prepare_directory tests passed!\n");
    
    // ==========================================
    // Test restore_directory function
    // ==========================================
    printf("Testing restore_directory function...\n");
    
    // Create test directory for restore_directory tests
    char *restore_test_dir_rel = "../tests/restore_test_dir";
    char *restore_output_dir = "restore_output_dir";
    
    mkdir("../tests", 0755);
    mkdir("../tests/restore_test_dir", 0755);
    mkdir("../tests/restore_test_dir/subdir", 0755);
    
    FILE *rf1 = fopen("../tests/restore_test_dir/file1.txt", "w");
    if (rf1) {
        fprintf(rf1, "This is file1 for restore_directory test.\n");
        fclose(rf1);
    }
    FILE *rf2 = fopen("../tests/restore_test_dir/subdir/file2.txt", "w");
    if (rf2) {
        fprintf(rf2, "This is file2 for restore_directory test.\n");
        fclose(rf2);
    }
    
    // Get absolute path to the test directory for proper serialization
    char restore_abs_path[1024];
    char restore_saved_cwd[1024];
    if (getcwd(restore_saved_cwd, sizeof(restore_saved_cwd)) == NULL) {
        perror("getcwd error");
        return 1;
    }
    if (chdir(restore_test_dir_rel) != 0) {
        perror("chdir error");
        return 1;
    }
    if (getcwd(restore_abs_path, sizeof(restore_abs_path)) == NULL) {
        perror("getcwd error");
        return 1;
    }
    if (chdir(restore_saved_cwd) != 0) {
        perror("chdir error");
        return 1;
    }
    
    // Test 1: Basic restore_directory functionality
    printf("  Test 1: Basic restore_directory functionality...\n");
    {
        // First, use prepare_directory with absolute path to serialize the directory
        char *data = NULL;
        int directory_size = 0;
        int result = prepare_directory(restore_abs_path, &data, &directory_size);
        if (result < 0) {
            fprintf(stderr, "Error: prepare_directory failed, code: %d\n", result);
            return 1;
        }
        
        // Clean output directory
        remove_directory_recursive(restore_output_dir);
        
        // Use restore_directory to extract
        result = restore_directory(data, restore_output_dir, true, false);
        if (result != 0) {
            fprintf(stderr, "Error: restore_directory failed, code: %d\n", result);
            free(data);
            return 1;
        }
        
        // Extract directory name from restore_abs_path (last component after last '/')
        char *dir_name = strrchr(restore_abs_path, '/');
        dir_name = (dir_name != NULL) ? dir_name + 1 : restore_abs_path;
        char extracted_path[1024];
        snprintf(extracted_path, sizeof(extracted_path), "%s/%s", restore_output_dir, dir_name);
        
        // Verify extraction by comparing directories
        if (compare_directories(restore_test_dir_rel, extracted_path) != 0) {
            fprintf(stderr, "Error: Directories do not match after restore_directory\n");
            free(data);
            return 1;
        }
        
        printf("    Basic restore_directory test passed.\n");
        free(data);
        
        // Cleanup
        remove_directory_recursive(restore_output_dir);
    }
    
    // Test 2: restore_directory with NULL output path (restore to current directory)
    printf("  Test 2: restore_directory with NULL output path...\n");
    {
        char *data = NULL;
        int directory_size = 0;
        int result = prepare_directory(restore_abs_path, &data, &directory_size);
        if (result < 0) {
            fprintf(stderr, "Error: prepare_directory failed, code: %d\n", result);
            return 1;
        }
        
        // Extract directory name
        char *dir_name = strrchr(restore_abs_path, '/');
        dir_name = (dir_name != NULL) ? dir_name + 1 : restore_abs_path;
        
        // Clean any existing directory with same name in current directory
        remove_directory_recursive(dir_name);
        
        // Use restore_directory with NULL output
        result = restore_directory(data, NULL, true, false);
        if (result != 0) {
            fprintf(stderr, "Error: restore_directory with NULL output failed, code: %d\n", result);
            free(data);
            return 1;
        }
        
        // Verify extraction
        if (compare_directories(restore_test_dir_rel, dir_name) != 0) {
            fprintf(stderr, "Error: Directories do not match after restore_directory with NULL output\n");
            free(data);
            remove_directory_recursive(dir_name);
            return 1;
        }
        
        printf("    restore_directory with NULL output test passed.\n");
        free(data);
        
        // Cleanup
        remove_directory_recursive(dir_name);
    }
    
    // Test 3: restore_directory with force flag (overwrite existing)
    printf("  Test 3: restore_directory with force flag...\n");
    {
        char *data = NULL;
        int directory_size = 0;
        int result = prepare_directory(restore_abs_path, &data, &directory_size);
        if (result < 0) {
            fprintf(stderr, "Error: prepare_directory failed, code: %d\n", result);
            return 1;
        }
        
        // Clean output directory and create fresh
        remove_directory_recursive(restore_output_dir);
        
        // First extraction
        result = restore_directory(data, restore_output_dir, true, false);
        if (result != 0) {
            fprintf(stderr, "Error: first restore_directory failed, code: %d\n", result);
            free(data);
            return 1;
        }
        
        // Second extraction with force flag (should overwrite)
        result = restore_directory(data, restore_output_dir, true, false);
        if (result != 0) {
            fprintf(stderr, "Error: second restore_directory with force failed, code: %d\n", result);
            free(data);
            return 1;
        }
        
        printf("    restore_directory with force flag test passed.\n");
        free(data);
        
        // Cleanup
        remove_directory_recursive(restore_output_dir);
    }
    
    // Cleanup test directory
    remove_directory_recursive(restore_test_dir_rel);
    
    printf("All restore_directory tests passed!\n");

    // ==========================================
    // Test directory permissions preservation
    // ==========================================
    printf("Testing directory permissions preservation...\n");
    
    // Create test directories with specific non-default permissions
    char *perm_test_dir = "../tests/perm_test_dir";
    char *perm_output_dir = "perm_output_dir";
    
    // Create test directory structure with non-default permissions
    mkdir("../tests", 0755);
    mkdir("../tests/perm_test_dir", 0755);
    mkdir("../tests/perm_test_dir/subdir_700", 0700);  // Owner only (rwx------)
    mkdir("../tests/perm_test_dir/subdir_750", 0750);  // Owner + group read (rwxr-x---)
    mkdir("../tests/perm_test_dir/subdir_755", 0755);  // Standard permissions (rwxr-xr-x)
    
    // Create test files in the directories
    FILE *perm_f1 = fopen("../tests/perm_test_dir/file1.txt", "w");
    if (perm_f1) {
        fprintf(perm_f1, "File in root directory.\n");
        fclose(perm_f1);
    }
    FILE *perm_f2 = fopen("../tests/perm_test_dir/subdir_700/file2.txt", "w");
    if (perm_f2) {
        fprintf(perm_f2, "File in subdir_700.\n");
        fclose(perm_f2);
    }
    FILE *perm_f3 = fopen("../tests/perm_test_dir/subdir_750/file3.txt", "w");
    if (perm_f3) {
        fprintf(perm_f3, "File in subdir_750.\n");
        fclose(perm_f3);
    }
    FILE *perm_f4 = fopen("../tests/perm_test_dir/subdir_755/file4.txt", "w");
    if (perm_f4) {
        fprintf(perm_f4, "File in subdir_755.\n");
        fclose(perm_f4);
    }
    
    // Get original permissions using stat()
    struct stat perm_st_700, perm_st_750, perm_st_755;
    int perm_700_mode = 0, perm_750_mode = 0, perm_755_mode = 0;
    
    if (stat("../tests/perm_test_dir/subdir_700", &perm_st_700) == 0) {
        perm_700_mode = perm_st_700.st_mode & 0777;
    } else {
        fprintf(stderr, "Error: Could not stat subdir_700\n");
        return 1;
    }
    if (stat("../tests/perm_test_dir/subdir_750", &perm_st_750) == 0) {
        perm_750_mode = perm_st_750.st_mode & 0777;
    } else {
        fprintf(stderr, "Error: Could not stat subdir_750\n");
        return 1;
    }
    if (stat("../tests/perm_test_dir/subdir_755", &perm_st_755) == 0) {
        perm_755_mode = perm_st_755.st_mode & 0777;
    } else {
        fprintf(stderr, "Error: Could not stat subdir_755\n");
        return 1;
    }
    
    printf("  Original permissions: subdir_700=%04o, subdir_750=%04o, subdir_755=%04o\n", 
           perm_700_mode, perm_750_mode, perm_755_mode);
    
    // Test: Archive, serialize, deserialize, and extract with permissions verification
    printf("  Test: Full round-trip preserving directory permissions...\n");
    {
        int perm_archive_size = 0;
        long perm_dir_size = 0;

        char perm_saved_cwd[1024];
        if (getcwd(perm_saved_cwd, sizeof(perm_saved_cwd)) == NULL) {
            perror("getcwd error");
            return 1;
        }
        
        if (chdir(perm_test_dir) != 0) {
            perror("chdir error");
            return 1;
        }

        char *perm_buffer = NULL;
        long perm_buffer_size = serialize_archive_to_buffer(".", &perm_buffer, &perm_archive_size, &perm_dir_size);

        if (chdir(perm_saved_cwd) != 0) {
            perror("chdir error");
            return 1;
        }
        if (perm_buffer_size < 0) {
            fprintf(stderr, "Error: Serialization failed with code %ld\n", perm_buffer_size);
            return 1;
        }
        
        // Deserialize the archive
        Directory_item *perm_deserialized = NULL;
        int perm_deser_size = deserialize_archive(&perm_deserialized, perm_buffer);
        if (perm_deser_size < 0) {
            fprintf(stderr, "Error: Deserialization failed with code %d\n", perm_deser_size);
            free(perm_buffer);
            return 1;
        }
        
        // Verify permissions were preserved after deserialization
        for (int i = 0; i < perm_deser_size; i++) {
            if (perm_deserialized[i].is_dir) {
                printf("    Deserialized dir: %s with perms=%04o\n", 
                       perm_deserialized[i].dir_path, perm_deserialized[i].perms);
            }
        }
        
        // Extract the directory
        remove_directory_recursive(perm_output_dir);
        mkdir(perm_output_dir, 0755);
        
        if (extract_directory(perm_output_dir, perm_deserialized, perm_deser_size, true, false) != 0) {
            fprintf(stderr, "Error: Extraction failed\n");
            free(perm_buffer);
            for (int i = 0; i < perm_deser_size; i++) {
                if (perm_deserialized[i].is_dir) {
                    free(perm_deserialized[i].dir_path);
                } else {
                    free(perm_deserialized[i].file_path);
                    free(perm_deserialized[i].file_data);
                }
            }
            free(perm_deserialized);
            return 1;
        }
        
        // Verify extracted directory permissions using stat()
        struct stat ext_st_700, ext_st_750, ext_st_755;
        int ext_700_mode = 0, ext_750_mode = 0, ext_755_mode = 0;
        
        char ext_700_path[256], ext_750_path[256], ext_755_path[256];
        snprintf(ext_700_path, sizeof(ext_700_path), "%s/subdir_700", perm_output_dir);
        snprintf(ext_750_path, sizeof(ext_750_path), "%s/subdir_750", perm_output_dir);
        snprintf(ext_755_path, sizeof(ext_755_path), "%s/subdir_755", perm_output_dir);
        
        if (stat(ext_700_path, &ext_st_700) == 0) {
            ext_700_mode = ext_st_700.st_mode & 0777;
        } else {
            fprintf(stderr, "Error: Could not stat extracted subdir_700\n");
            return 1;
        }
        if (stat(ext_750_path, &ext_st_750) == 0) {
            ext_750_mode = ext_st_750.st_mode & 0777;
        } else {
            fprintf(stderr, "Error: Could not stat extracted subdir_750\n");
            return 1;
        }
        if (stat(ext_755_path, &ext_st_755) == 0) {
            ext_755_mode = ext_st_755.st_mode & 0777;
        } else {
            fprintf(stderr, "Error: Could not stat extracted subdir_755\n");
            return 1;
        }
        
        printf("  Extracted permissions: subdir_700=%04o, subdir_750=%04o, subdir_755=%04o\n", 
               ext_700_mode, ext_750_mode, ext_755_mode);
        
        // Compare permissions
        if (perm_700_mode != ext_700_mode) {
            fprintf(stderr, "Error: subdir_700 permissions mismatch! Original: %04o, Extracted: %04o\n",
                    perm_700_mode, ext_700_mode);
            return 1;
        }
        if (perm_750_mode != ext_750_mode) {
            fprintf(stderr, "Error: subdir_750 permissions mismatch! Original: %04o, Extracted: %04o\n",
                    perm_750_mode, ext_750_mode);
            return 1;
        }
        if (perm_755_mode != ext_755_mode) {
            fprintf(stderr, "Error: subdir_755 permissions mismatch! Original: %04o, Extracted: %04o\n",
                    perm_755_mode, ext_755_mode);
            return 1;
        }
        
        printf("    All directory permissions preserved correctly!\n");

        // Cleanup
        free(perm_buffer);
        for (int i = 0; i < perm_deser_size; i++) {
            if (perm_deserialized[i].is_dir) {
                free(perm_deserialized[i].dir_path);
            } else {
                free(perm_deserialized[i].file_path);
                free(perm_deserialized[i].file_data);
            }
        }
        free(perm_deserialized);
        
        remove_directory_recursive(perm_output_dir);
    }
    
    // Cleanup test directory
    remove_directory_recursive(perm_test_dir);
    
    printf("All directory permissions tests passed!\n");

    // ==========================================
    // EDGE CASE TESTS
    // ==========================================
    printf("Testing edge cases...\n");
    
    // Edge case 1: Deep nested directory structure
    printf("  Edge case 1: Deep nested directory...\n");
    {
        char *deep_test_dir = "../tests/deep_test_dir";
        char *deep_output_dir = "deep_output_dir";
        
        mkdir("../tests", 0755);
        mkdir(deep_test_dir, 0755);
        
        // Create 10 levels deep
        char path[1024];
        strcpy(path, deep_test_dir);
        for (int i = 0; i < 10; i++) {
            snprintf(path + strlen(path), sizeof(path) - strlen(path), "/level%d", i);
            mkdir(path, 0755);
        }
        
        // Create file at deepest level
        char file_path[1024];
        snprintf(file_path, sizeof(file_path), "%s/deepfile.txt", path);
        FILE *df = fopen(file_path, "w");
        if (df) {
            fprintf(df, "Deep file content.\n");
            fclose(df);
        }
        
        // Archive, serialize, deserialize, extract
        int deep_archive_size = 0;
        long deep_dir_size = 0;

        char deep_cwd[1024];
        getcwd(deep_cwd, sizeof(deep_cwd));
        chdir(deep_test_dir);
        char *deep_buffer = NULL;
        long deep_buffer_size = serialize_archive_to_buffer(".", &deep_buffer, &deep_archive_size, &deep_dir_size);
        chdir(deep_cwd);
        assert(deep_buffer_size > 0);
        
        Directory_item *deep_deserialized = NULL;
        int deep_deser_size = deserialize_archive(&deep_deserialized, deep_buffer);
        assert(deep_deser_size > 0);
        
        remove_directory_recursive(deep_output_dir);
        mkdir(deep_output_dir, 0755);
        
        assert(extract_directory(deep_output_dir, deep_deserialized, deep_deser_size, true, false) == 0);
        assert(compare_directories(deep_test_dir, deep_output_dir) == 0);
        
        // Cleanup
        free(deep_buffer);
        free_directory_items(deep_deserialized, deep_deser_size);
        remove_directory_recursive(deep_test_dir);
        remove_directory_recursive(deep_output_dir);
        
        printf("    Deep nested directory test passed.\n");
    }
    
    // Edge case 2: Directory with many files
    printf("  Edge case 2: Directory with many files...\n");
    {
        char *many_test_dir = "../tests/many_files_dir";
        char *many_output_dir = "many_files_output";
        
        mkdir("../tests", 0755);
        mkdir(many_test_dir, 0755);
        
        // Create 100 files
        for (int i = 0; i < 100; i++) {
            char file_path[1024];
            snprintf(file_path, sizeof(file_path), "%s/file%03d.txt", many_test_dir, i);
            FILE *mf = fopen(file_path, "w");
            if (mf) {
                fprintf(mf, "Content of file %d\n", i);
                fclose(mf);
            }
        }
        
        int many_archive_size = 0;
        long many_dir_size = 0;

        char many_cwd[1024];
        getcwd(many_cwd, sizeof(many_cwd));
        chdir(many_test_dir);
        char *many_buffer = NULL;
        long many_buffer_size = serialize_archive_to_buffer(".", &many_buffer, &many_archive_size, &many_dir_size);
        chdir(many_cwd);
        assert(many_buffer_size > 0);
        
        Directory_item *many_deserialized = NULL;
        int many_deser_size = deserialize_archive(&many_deserialized, many_buffer);
        assert(many_deser_size > 0);
        
        remove_directory_recursive(many_output_dir);
        mkdir(many_output_dir, 0755);
        
        assert(extract_directory(many_output_dir, many_deserialized, many_deser_size, true, false) == 0);
        assert(compare_directories(many_test_dir, many_output_dir) == 0);
        
        // Cleanup
        free(many_buffer);
        free_directory_items(many_deserialized, many_deser_size);
        remove_directory_recursive(many_test_dir);
        remove_directory_recursive(many_output_dir);
        
        printf("    Directory with many files test passed.\n");
    }
    
    // Edge case 3: Files with special characters in names
    printf("  Edge case 3: Files with special characters...\n");
    {
        char *special_test_dir = "../tests/special_chars_dir";
        char *special_output_dir = "special_chars_output";
        
        mkdir("../tests", 0755);
        mkdir(special_test_dir, 0755);
        
        // Create files with special characters (safe ones)
        char *special_names[] = {
            "file-with-dashes.txt",
            "file_with_underscores.txt",
            "file.with.dots.txt",
            "file123numbers.txt"
        };
        
        for (int i = 0; i < 4; i++) {
            char file_path[1024];
            snprintf(file_path, sizeof(file_path), "%s/%s", special_test_dir, special_names[i]);
            FILE *sf = fopen(file_path, "w");
            if (sf) {
                fprintf(sf, "Special file %d\n", i);
                fclose(sf);
            }
        }
        
        int special_archive_size = 0;
        long special_dir_size = 0;

        char special_cwd[1024];
        getcwd(special_cwd, sizeof(special_cwd));
        chdir(special_test_dir);
        char *special_buffer = NULL;
        long special_buffer_size = serialize_archive_to_buffer(".", &special_buffer, &special_archive_size, &special_dir_size);
        chdir(special_cwd);
        assert(special_buffer_size > 0);
        
        Directory_item *special_deserialized = NULL;
        int special_deser_size = deserialize_archive(&special_deserialized, special_buffer);
        assert(special_deser_size > 0);
        
        remove_directory_recursive(special_output_dir);
        mkdir(special_output_dir, 0755);
        
        assert(extract_directory(special_output_dir, special_deserialized, special_deser_size, true, false) == 0);
        assert(compare_directories(special_test_dir, special_output_dir) == 0);
        
        // Cleanup
        free(special_buffer);
        free_directory_items(special_deserialized, special_deser_size);
        remove_directory_recursive(special_test_dir);
        remove_directory_recursive(special_output_dir);
        
        printf("    Files with special characters test passed.\n");
    }
    
    // Edge case 4: Large files in directory
    printf("  Edge case 4: Large files in directory...\n");
    {
        char *large_test_dir = "../tests/large_files_dir";
        char *large_output_dir = "large_files_output";
        
        mkdir("../tests", 0755);
        mkdir(large_test_dir, 0755);
        
        // Create one large file
        char large_file[1024];
        snprintf(large_file, sizeof(large_file), "%s/largefile.txt", large_test_dir);
        FILE *lf = fopen(large_file, "w");
        if (lf) {
            debugmalloc_max_block_size(10 * 1024 * 1024);  // 10MB
            for (int i = 0; i < 10000; i++) {
                fprintf(lf, "This is line %d of the large file with some content.\n", i);
            }
            fclose(lf);
        }
        
        int large_archive_size = 0;
        long large_dir_size = 0;

        char large_cwd[1024];
        getcwd(large_cwd, sizeof(large_cwd));
        chdir(large_test_dir);
        char *large_buffer = NULL;
        long large_buffer_size = serialize_archive_to_buffer(".", &large_buffer, &large_archive_size, &large_dir_size);
        chdir(large_cwd);
        assert(large_buffer_size > 0);
        
        Directory_item *large_deserialized = NULL;
        int large_deser_size = deserialize_archive(&large_deserialized, large_buffer);
        assert(large_deser_size > 0);
        
        remove_directory_recursive(large_output_dir);
        mkdir(large_output_dir, 0755);
        
        assert(extract_directory(large_output_dir, large_deserialized, large_deser_size, true, false) == 0);
        assert(compare_directories(large_test_dir, large_output_dir) == 0);
        
        // Cleanup
        free(large_buffer);
        free_directory_items(large_deserialized, large_deser_size);
        remove_directory_recursive(large_test_dir);
        remove_directory_recursive(large_output_dir);
        
        printf("    Large files in directory test passed.\n");
    }
    
    // Edge case 5: Empty files in directory
    printf("  Edge case 5: Empty files in directory...\n");
    {
        char *empty_test_dir = "../tests/empty_files_dir";
        char *empty_output_dir = "empty_files_output";
        
        mkdir("../tests", 0755);
        mkdir(empty_test_dir, 0755);
        
        // Create empty files
        for (int i = 0; i < 5; i++) {
            char file_path[1024];
            snprintf(file_path, sizeof(file_path), "%s/empty%d.txt", empty_test_dir, i);
            FILE *ef = fopen(file_path, "w");
            if (ef) {
                fclose(ef);
            }
        }
        
        int empty_archive_size = 0;
        long empty_dir_size = 0;

        char empty_cwd[1024];
        getcwd(empty_cwd, sizeof(empty_cwd));
        chdir(empty_test_dir);
        char *empty_buffer = NULL;
        long empty_buffer_size = serialize_archive_to_buffer(".", &empty_buffer, &empty_archive_size, &empty_dir_size);
        chdir(empty_cwd);
        assert(empty_buffer_size > 0);
        
        Directory_item *empty_deserialized = NULL;
        int empty_deser_size = deserialize_archive(&empty_deserialized, empty_buffer);
        assert(empty_deser_size > 0);
        
        remove_directory_recursive(empty_output_dir);
        mkdir(empty_output_dir, 0755);
        
        assert(extract_directory(empty_output_dir, empty_deserialized, empty_deser_size, true, false) == 0);
        assert(compare_directories(empty_test_dir, empty_output_dir) == 0);
        
        // Cleanup
        free(empty_buffer);
        free_directory_items(empty_deserialized, empty_deser_size);
        remove_directory_recursive(empty_test_dir);
        remove_directory_recursive(empty_output_dir);
        
        printf("    Empty files in directory test passed.\n");
    }
    
    // Edge case 6: Binary files in directory
    printf("  Edge case 6: Binary files in directory...\n");
    {
        char *binary_test_dir = "../tests/binary_files_dir";
        char *binary_output_dir = "binary_files_output";
        
        mkdir("../tests", 0755);
        mkdir(binary_test_dir, 0755);
        
        // Create binary file
        char binary_file[1024];
        snprintf(binary_file, sizeof(binary_file), "%s/binary.dat", binary_test_dir);
        FILE *bf = fopen(binary_file, "wb");
        if (bf) {
            unsigned char data[256];
            for (int i = 0; i < 256; i++) {
                data[i] = (unsigned char)i;
            }
            fwrite(data, 1, 256, bf);
            fclose(bf);
        }
        
        int binary_archive_size = 0;
        long binary_dir_size = 0;

        char binary_cwd[1024];
        getcwd(binary_cwd, sizeof(binary_cwd));
        chdir(binary_test_dir);
        char *binary_buffer = NULL;
        long binary_buffer_size = serialize_archive_to_buffer(".", &binary_buffer, &binary_archive_size, &binary_dir_size);
        chdir(binary_cwd);
        assert(binary_buffer_size > 0);
        
        Directory_item *binary_deserialized = NULL;
        int binary_deser_size = deserialize_archive(&binary_deserialized, binary_buffer);
        assert(binary_deser_size > 0);
        
        remove_directory_recursive(binary_output_dir);
        mkdir(binary_output_dir, 0755);
        
        assert(extract_directory(binary_output_dir, binary_deserialized, binary_deser_size, true, false) == 0);
        assert(compare_directories(binary_test_dir, binary_output_dir) == 0);
        
        // Cleanup
        free(binary_buffer);
        free_directory_items(binary_deserialized, binary_deser_size);
        remove_directory_recursive(binary_test_dir);
        remove_directory_recursive(binary_output_dir);
        
        printf("    Binary files in directory test passed.\n");
    }
    
    printf("All edge case tests passed!\n");

    return 0;
}
