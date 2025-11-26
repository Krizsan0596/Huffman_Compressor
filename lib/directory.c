#include "directory.h"
#include "data_types.h"
#include "file.h"
#include "debugmalloc.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

long archive_directory(char *path, Directory_item **archive, int *current_index, int *archive_size) {
    if (*current_index == 0) {
        Directory_item root = {0};
        root.is_dir = true;
        root.dir_path = strdup(path);
        if (root.dir_path == NULL) return MALLOC_ERROR;
        Directory_item *temp = realloc(*archive, (*archive_size + 1) * sizeof(Directory_item));
        if (temp != NULL) *archive = temp;
        else {
            free(root.dir_path);
            return MALLOC_ERROR;
        }
        (*archive_size)++;
        (*archive)[(*current_index)++] = root;
    }

    DIR *directory = opendir(path);
    if (directory == NULL) return DIRECTORY_OPEN_ERROR;
    long dir_size = 0;
    while (true) {
        struct dirent *dir = readdir(directory);
        if (dir == NULL) break;
        else if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
        char *newpath = malloc(strlen(path) + strlen(dir->d_name) + 2);
        if (newpath == NULL) return MALLOC_ERROR;
        strcpy(newpath, path);
        strcat(newpath, "/");
        strcat(newpath, dir->d_name);

        struct stat st;
        if (stat(newpath, &st) != 0) {
            free(newpath);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            Directory_item subdir = {0};
            subdir.is_dir = true;
            subdir.dir_path = strdup(newpath);
            if (subdir.dir_path == NULL) return MALLOC_ERROR;
            Directory_item *temp = realloc(*archive, (*archive_size + 1) * sizeof(Directory_item));
            if (temp != NULL) *archive = temp;
            else {
                free(newpath);
                free(subdir.dir_path);
                return MALLOC_ERROR;
            }
            (*archive_size)++;
            (*archive)[(*current_index)++] = subdir;
            long subdir_size = archive_directory(newpath, archive, current_index, archive_size);
            if (subdir_size < 0) {
                free(newpath);
                return subdir_size;
            }
            dir_size += subdir_size;
        } 
        else if (S_ISREG(st.st_mode)) {
            Directory_item file = {0};
            file.is_dir = false;
            file.file_path = strdup(newpath);
            if (file.file_path == NULL) return MALLOC_ERROR;
            file.file_size = read_raw(newpath, &file.file_data);
            dir_size += file.file_size;
            if (file.file_size < 0) {
                free(file.file_path);
                return FILE_READ_ERROR;
            } 
            Directory_item *temp = realloc(*archive, (*archive_size + 1) * sizeof(Directory_item));
            if (temp != NULL) *archive = temp;
            else {
                free(newpath);
                free(file.file_path);
                free(file.file_data);
                return MALLOC_ERROR;
            }

            (*archive_size)++;
            (*archive)[(*current_index)++] = file;
        }
        free(newpath);
    }
    closedir(directory);
    return dir_size;
}

long serialize_archive(Directory_item *archive, int archive_size, char **buffer) {
    if (archive_size == 0) return EMPTY_DIRECTORY;
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
    if (*buffer == NULL) return MALLOC_ERROR;
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
    if (path == NULL) path = ".";
    for (int current_index = 0; current_index < archive_size; current_index++) {
        Directory_item *current = &archive[current_index];
        char *item_path = current->is_dir ? current->dir_path : current->file_path;
        char *full_path = malloc(strlen(path) + strlen(item_path) + 2);
        if (full_path == NULL) return MALLOC_ERROR;
        strcpy(full_path, path);
        strcat(full_path, "/");
        strcat(full_path, item_path);
        if (current->is_dir) {
           int ret = mkdir(full_path, 0755);
           if (ret != 0 && errno != EEXIST) return MKDIR_ERROR;
        }
        else {
            int ret = write_raw(full_path, current->file_data, current->file_size, force);
            if (ret < 0) return FILE_WRITE_ERROR;
        }
        free(full_path);
    }
    return SUCCESS;
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
            memcpy((*archive)[i].dir_path, current, n);
            current += n;
        }
        else {
            memcpy(&(*archive)[i].file_size, current, sizeof(long));
            current += sizeof(long);
            char *end = strchr(current, '\0');
            int n = end - current + 1;
            (*archive)[i].file_path = malloc(n);
            if ((*archive)[i].file_path == NULL) return MALLOC_ERROR;
            memcpy((*archive)[i].file_path, current, n);
            current += n;
            (*archive)[i].file_data = malloc((*archive)[i].file_size);
            if ((*archive)[i].file_data == NULL) return MALLOC_ERROR;
            memcpy((*archive)[i].file_data, current, (*archive)[i].file_size);
            current += (*archive)[i].file_size;
        }
        i++;
    }
    return archive_size;
}
