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
    int ret = system(command);
    (void)ret;
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
    ret = system(command);
    (void)ret;
    snprintf(command, sizeof(command), "rm -rf %s", test_dir);
    ret = system(command);
    (void)ret;

    return 0;
}
