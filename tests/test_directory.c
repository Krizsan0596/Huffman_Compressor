#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include "../lib/directory.h"
#include "../lib/data_types.h"
#include "../lib/debugmalloc.h"

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
    char command[1024];
    snprintf(command, sizeof(command), "rm -rf %s", output_dir);
    system(command);
    mkdir(output_dir, 0755);

    // 1. Archive the directory
    Directory_item *archive = NULL;
    int archive_size = 0;
    int current_index = 0;

    char original_cwd[1024];
    if (getcwd(original_cwd, sizeof(original_cwd)) == NULL) {
        perror("getcwd() error");
        return 1;
    }

    if (chdir(test_dir) != 0) {
        perror("chdir() error");
        return 1;
    }

    long dir_size = archive_directory(".", &archive, &current_index, &archive_size);

    if (chdir(original_cwd) != 0) {
        perror("chdir() error");
        return 1;
    }

    if (dir_size < 0) {
        fprintf(stderr, "Error: Archiving failed with code %ld\n", dir_size);
        return 1;
    }

    // 2. Serialize the archive
    char *buffer = NULL;
    long buffer_size = serialize_archive(archive, archive_size, &buffer);
    if (buffer_size < 0) {
        fprintf(stderr, "Error: Serialization failed with code %ld\n", buffer_size);
        free(archive);
        return 1;
    }

    // 3. Deserialize the archive
    Directory_item *deserialized_archive = NULL;
    int deserialized_size = deserialize_archive(&deserialized_archive, buffer);
    if (deserialized_size < 0) {
        fprintf(stderr, "Error: Deserialization failed with code %d\n", deserialized_size);
        free(buffer);
        free(archive);
        return 1;
    }

    // 4. Extract the directory
    if (chdir(output_dir) != 0) {
        perror("chdir() error");
        return 1;
    }
    if (extract_directory(".", deserialized_archive, deserialized_size, true) != 0) {
        if (chdir(original_cwd) != 0) {
            perror("chdir() error");
        }
        fprintf(stderr, "Error: Extraction failed\n");
        free(buffer);
        free(archive);
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
        free(archive);
        free(deserialized_archive);
        return 1;
    }


    // 6. Cleanup
    free(buffer);
    for (int i = 0; i < archive_size; i++) {
        if (archive[i].is_dir) {
            free(archive[i].dir_path);
        }
    }
    for (int i = 0; i < archive_size; i++) {
        if (!archive[i].is_dir) {
            free(archive[i].file_path);
            free(archive[i].file_data);
        }
    }
    free(archive);
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

    snprintf(command, sizeof(command), "rm -rf %s", output_dir);
    system(command);
    snprintf(command, sizeof(command), "rm -rf %s", test_dir);
    system(command);

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
        snprintf(command, sizeof(command), "rm -rf %s", prep_output_dir);
        system(command);
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
        
        if (extract_directory(".", prep_archive, prep_archive_size, true) != 0) {
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
        snprintf(command, sizeof(command), "rm -rf %s", prep_output_dir);
        system(command);
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
    snprintf(command, sizeof(command), "rm -rf %s", prep_test_dir);
    system(command);
    
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
        snprintf(command, sizeof(command), "rm -rf %s", restore_output_dir);
        system(command);
        
        // Use restore_directory to extract
        result = restore_directory(data, restore_output_dir, true);
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
        snprintf(command, sizeof(command), "rm -rf %s", restore_output_dir);
        system(command);
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
        snprintf(command, sizeof(command), "rm -rf ./%s", dir_name);
        system(command);
        
        // Use restore_directory with NULL output
        result = restore_directory(data, NULL, true);
        if (result != 0) {
            fprintf(stderr, "Error: restore_directory with NULL output failed, code: %d\n", result);
            free(data);
            return 1;
        }
        
        // Verify extraction
        if (compare_directories(restore_test_dir_rel, dir_name) != 0) {
            fprintf(stderr, "Error: Directories do not match after restore_directory with NULL output\n");
            free(data);
            snprintf(command, sizeof(command), "rm -rf ./%s", dir_name);
            system(command);
            return 1;
        }
        
        printf("    restore_directory with NULL output test passed.\n");
        free(data);
        
        // Cleanup
        snprintf(command, sizeof(command), "rm -rf ./%s", dir_name);
        system(command);
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
        snprintf(command, sizeof(command), "rm -rf %s", restore_output_dir);
        system(command);
        
        // First extraction
        result = restore_directory(data, restore_output_dir, true);
        if (result != 0) {
            fprintf(stderr, "Error: first restore_directory failed, code: %d\n", result);
            free(data);
            return 1;
        }
        
        // Second extraction with force flag (should overwrite)
        result = restore_directory(data, restore_output_dir, true);
        if (result != 0) {
            fprintf(stderr, "Error: second restore_directory with force failed, code: %d\n", result);
            free(data);
            return 1;
        }
        
        printf("    restore_directory with force flag test passed.\n");
        free(data);
        
        // Cleanup
        snprintf(command, sizeof(command), "rm -rf %s", restore_output_dir);
        system(command);
    }
    
    // Cleanup test directory
    snprintf(command, sizeof(command), "rm -rf %s", restore_test_dir_rel);
    system(command);
    
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
        // Archive the directory
        Directory_item *perm_archive = NULL;
        int perm_archive_size = 0;
        int perm_current_index = 0;
        
        char perm_saved_cwd[1024];
        if (getcwd(perm_saved_cwd, sizeof(perm_saved_cwd)) == NULL) {
            perror("getcwd error");
            return 1;
        }
        
        if (chdir(perm_test_dir) != 0) {
            perror("chdir error");
            return 1;
        }
        
        long perm_dir_size = archive_directory(".", &perm_archive, &perm_current_index, &perm_archive_size);
        
        if (chdir(perm_saved_cwd) != 0) {
            perror("chdir error");
            return 1;
        }
        
        if (perm_dir_size < 0) {
            fprintf(stderr, "Error: Archiving failed with code %ld\n", perm_dir_size);
            return 1;
        }
        
        // Verify permissions were captured in archive
        for (int i = 0; i < perm_archive_size; i++) {
            if (perm_archive[i].is_dir) {
                printf("    Archived dir: %s with perms=%04o\n", 
                       perm_archive[i].dir_path, perm_archive[i].perms);
            }
        }
        
        // Serialize the archive
        char *perm_buffer = NULL;
        long perm_buffer_size = serialize_archive(perm_archive, perm_archive_size, &perm_buffer);
        if (perm_buffer_size < 0) {
            fprintf(stderr, "Error: Serialization failed with code %ld\n", perm_buffer_size);
            for (int i = 0; i < perm_archive_size; i++) {
                if (perm_archive[i].is_dir) {
                    free(perm_archive[i].dir_path);
                } else {
                    free(perm_archive[i].file_path);
                    free(perm_archive[i].file_data);
                }
            }
            free(perm_archive);
            return 1;
        }
        
        // Deserialize the archive
        Directory_item *perm_deserialized = NULL;
        int perm_deser_size = deserialize_archive(&perm_deserialized, perm_buffer);
        if (perm_deser_size < 0) {
            fprintf(stderr, "Error: Deserialization failed with code %d\n", perm_deser_size);
            free(perm_buffer);
            for (int i = 0; i < perm_archive_size; i++) {
                if (perm_archive[i].is_dir) {
                    free(perm_archive[i].dir_path);
                } else {
                    free(perm_archive[i].file_path);
                    free(perm_archive[i].file_data);
                }
            }
            free(perm_archive);
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
        snprintf(command, sizeof(command), "rm -rf %s", perm_output_dir);
        system(command);
        mkdir(perm_output_dir, 0755);
        
        if (extract_directory(perm_output_dir, perm_deserialized, perm_deser_size, true) != 0) {
            fprintf(stderr, "Error: Extraction failed\n");
            free(perm_buffer);
            for (int i = 0; i < perm_archive_size; i++) {
                if (perm_archive[i].is_dir) {
                    free(perm_archive[i].dir_path);
                } else {
                    free(perm_archive[i].file_path);
                    free(perm_archive[i].file_data);
                }
            }
            free(perm_archive);
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
        for (int i = 0; i < perm_archive_size; i++) {
            if (perm_archive[i].is_dir) {
                free(perm_archive[i].dir_path);
            } else {
                free(perm_archive[i].file_path);
                free(perm_archive[i].file_data);
            }
        }
        free(perm_archive);
        for (int i = 0; i < perm_deser_size; i++) {
            if (perm_deserialized[i].is_dir) {
                free(perm_deserialized[i].dir_path);
            } else {
                free(perm_deserialized[i].file_path);
                free(perm_deserialized[i].file_data);
            }
        }
        free(perm_deserialized);
        
        snprintf(command, sizeof(command), "rm -rf %s", perm_output_dir);
        system(command);
    }
    
    // Cleanup test directory
    snprintf(command, sizeof(command), "rm -rf %s", perm_test_dir);
    system(command);
    
    printf("All directory permissions tests passed!\n");

    return 0;
}
