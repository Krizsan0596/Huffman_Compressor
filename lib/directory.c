#include "directory.h"
#include "data_types.h"
#include "file.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

int archive_directory(char *path, Directory_item **archive, int *current, int *archive_size) {
    DIR *directory = opendir(path);
    if (directory == NULL) return -1;
    while (true) {
        struct dirent *dir = readdir(directory);
        if (dir == NULL) break;
        else if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
        char *newpath = malloc(strlen(path) + strlen(dir->d_name) + 1);
        strcpy(newpath, path);
        strcat(newpath, dir->d_name);
        if (dir->d_type == DT_DIR) {
            Directory_item subdir = {0};
            subdir.is_dir = true;
            subdir.dir_name = strdup(dir->d_name);
            Directory_item *temp = realloc(*archive, (*archive_size + 1) * sizeof(Directory_item));
            if (temp != NULL) *archive = temp;
            else return -2;
            (*archive_size)++;
            memcpy(archive[*current++], &subdir, sizeof(Directory_item));
            int ret = archive_directory(newpath, archive, current, archive_size);
            if (ret != 0) {
                free(newpath);
                return ret;
            }
        } 
        else if (dir->d_type == DT_REG) {
            Directory_item file = {0};
            file.is_dir = false;
            file.file_path = strdup(newpath);
            file.file_size = read_raw(newpath, &file.file_data);
            if (file.file_size < 0) return -3;
            Directory_item *temp = realloc(*archive, (*archive_size + 1) * sizeof(Directory_item));
            if (temp != NULL) *archive = temp;
            else return -2;
            (*archive_size)++;
            memcpy(archive[*current++], &file, sizeof(Directory_item));
        }
        free(newpath);
    }
    closedir(directory);
    return 0;
}
