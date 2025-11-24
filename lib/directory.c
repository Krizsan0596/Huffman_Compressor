#include "directory.h"
#include "data_types.h"
#include "file.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

long archive_directory(char *path, Directory_item **archive, int *current_index, int *archive_size) {
    DIR *directory = opendir(path);
    if (directory == NULL) return -1;
    long dir_size = 0;
    while (true) {
        struct dirent *dir = readdir(directory);
        if (dir == NULL) break;
        else if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
        char *newpath = malloc(strlen(path) + strlen(dir->d_name) + 2);
        if (newpath == NULL) return -1;
        strcpy(newpath, path);
        strcat(newpath, "/");
        strcat(newpath, dir->d_name);

        if (dir->d_type == DT_DIR) {
            Directory_item subdir = {0};
            subdir.is_dir = true;
            subdir.dir_path = strdup(newpath);
            if (subdir.dir_path == NULL) return -1;
            Directory_item *temp = realloc(*archive, (*archive_size + 1) * sizeof(Directory_item));
            if (temp != NULL) *archive = temp;
            else {
                free(newpath);
                free(subdir.dir_path);
                return -2;
            }
            (*archive_size)++;
            memcpy(&(*archive)[(*current_index)++], &subdir, sizeof(Directory_item));
            int ret = archive_directory(newpath, archive, current_index, archive_size);
            if (ret != 0) {
                free(newpath);
                return ret;
            }
        } 
        else if (dir->d_type == DT_REG) {
            Directory_item file = {0};
            file.is_dir = false;
            file.file_path = strdup(newpath);
            if (file.file_path == NULL) return -1;
            file.file_size = read_raw(newpath, &file.file_data);
            dir_size += file.file_size;
            if (file.file_size < 0) {
                free(file.file_path);
                return -3;
            } 
            Directory_item *temp = realloc(*archive, (*archive_size + 1) * sizeof(Directory_item));
            if (temp != NULL) *archive = temp;
            else {
                free(newpath);
                free(file.file_path);
                return -2;
            }

            (*archive_size)++;
            memcpy(&(*archive)[(*current_index)++], &file, sizeof(Directory_item));
        }
        free(newpath);
    }
    closedir(directory);
    return dir_size;
}

long serialize_archive(Directory_item *archive, int archive_size, char **buffer) {
    if (archive_size == 0) return -2; // empty dir
    int data_size = sizeof(int);
    for (int i = 0; i < archive_size; i++) {
        data_size += sizeof(bool);
        if (archive[i].is_dir) {
            data_size += strlen(archive[i].dir_path) + 1;
        }
        else {
            data_size += sizeof(long);
            data_size += strlen(archive[i].file_path) + 1;
            data_size += archive[i].file_size;
        }
    }
    *buffer = malloc(data_size);
    if (*buffer == NULL) return -1; // malloc error
    char *current = *buffer;
    memcpy(current, &archive_size, sizeof(int));
    current += sizeof(int);
    for (int i = 0; i < archive_size; i++) {
        memcpy(current, &archive[i].is_dir, sizeof(bool));
        current += sizeof(bool);
        if (archive[i].is_dir) {
            memcpy(current, archive[i].dir_path, strlen(archive[i].dir_path) + 1);
            current += strlen(archive[i].dir_path) + 1;
        }
        else {
            memcpy(current, &archive[i].file_size, sizeof(long));
            current += sizeof(long);
            memcpy(current, archive[i].file_path, strlen(archive[i].file_path) + 1);
            current += strlen(archive[i].file_path) + 1;
            memcpy(current, archive[i].file_data, archive[i].file_size);
            current += archive[i].file_size;
        }
    }
    return data_size;
}


int extract_directory(char *path, Directory_item *archive, int archive_size, bool force) {
    for (int current_index = 0; current_index < archive_size; current_index++) {
        Directory_item *current = &archive[current_index];
        char *full_path = malloc(strlen(path) + (current->is_dir ? strlen(current->dir_path) : strlen(current->file_path)) + 2);
        if (full_path == NULL) return -1;
        
        strcpy(full_path, path);
        strcat(full_path, "/");
        strcat(full_path, current->is_dir ? current->dir_path : current->file_path);

        if (current->is_dir) {
           if (mkdir(full_path, 0755) != 0) {
               free(full_path);
               return -1;
           }
        }
        else {
            if (write_raw(full_path, current->file_data, current->file_size, force) != 0) {
                free(full_path);
                return -2;
            }
        }
        free(full_path);
    }
    return 0;
}

int deserialize_archive(Directory_item **archive, char *buffer) {
    int archive_size;
    char *current = buffer;
    memcpy(&archive_size, current, sizeof(int));
    current += sizeof(int);
    *archive = malloc(archive_size * sizeof(Directory_item));
    int i = 0;
    while (i < archive_size) {
        memcpy(&(*archive)[i].is_dir, current, sizeof(bool));
        current += sizeof(bool);
        if ((*archive)[i].is_dir) {
            char *end = strchr(current, '\0');
            int n = end - current + 1;
            (*archive)[i].dir_path = malloc(n);
            if ((*archive)[i].dir_path == NULL) return MALLOC_ERROR;
            memcpy(&(*archive)[i].dir_path, current, n);
            current += n;
        }
        else {
            memcpy(&(*archive)[i].file_size, current, sizeof(long));
            current += sizeof(long);
            char *end = strchr(current, '\0');
            int n = end - current + 1;
            (*archive)[i].file_path = malloc(n);
            if ((*archive)[i].file_path == NULL) return MALLOC_ERROR;
            memcpy(&(*archive)[i].file_path, current, n);
            current += n;
            (*archive)[i].file_data = malloc((*archive)[i].file_size);
            if ((*archive)[i].file_data == NULL) return -1;
            memcpy(&(*archive)[i].file_data, current, (*archive)[i].file_size);
            current += (*archive)[i].file_size;
        }
        i++;
    }
    return 0;
}
