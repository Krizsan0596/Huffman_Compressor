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
#include <stdio.h>

/*
 * Rekurzivan bejarja a mappat es fajlonkent egy tombbe menti az adatokat.
 * Siker eseten a mappa meretet adja vissza bajtokban, hiba eseten negativ kodot.
 */
long archive_directory(char *path, Directory_item **archive, int *current_index, int *archive_size) {
    DIR *directory = NULL;
    long dir_size = 0;
    long result = 0;
    char *newpath = NULL;
    Directory_item current_item = {0};
    
    while (true) {
        /* Az elso hivaskor felvesszuk a gyoker mappat az archivumba, hogy a relativ utak megmaradjanak. */
        if (*current_index == 0) {
            Directory_item root = {0};
            root.is_dir = true;
            root.dir_path = strdup(path);
            if (root.dir_path == NULL) {
                result = MALLOC_ERROR;
                break;
            }
            Directory_item *temp = realloc(*archive, (*archive_size + 1) * sizeof(Directory_item));
            if (temp != NULL) *archive = temp;
            else {
                result = MALLOC_ERROR;
                current_item = root;
                break;
            }
            (*archive_size)++;
            (*archive)[(*current_index)++] = root;
        }

        directory = opendir(path);
        if (directory == NULL) {
            if (*current_index == 1) {
                current_item = (*archive)[0];
                (*archive_size)--;
                (*current_index)--;
            }
            result = DIRECTORY_OPEN_ERROR;
            break;
        }
        
        while (true) {
            struct dirent *dir = readdir(directory);
            if (dir == NULL) break;
            else if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
            
            /* Osszerakjuk a talalt elem teljes eleresi utjat, hogy stat-tel tudjuk vizsgalni. */
            newpath = malloc(strlen(path) + strlen(dir->d_name) + 2);
            if (newpath == NULL) {
                result = MALLOC_ERROR;
                break;
            }
            strcpy(newpath, path);
            strcat(newpath, "/");
            strcat(newpath, dir->d_name);

            struct stat st;
            if (stat(newpath, &st) != 0) {
                free(newpath);
                newpath = NULL;
                continue;
            }

            if (S_ISDIR(st.st_mode)) {
                Directory_item subdir = {0};
                subdir.is_dir = true;
                subdir.dir_path = strdup(newpath);
                if (subdir.dir_path == NULL) {
                    result = MALLOC_ERROR;
                    break;
                }
                Directory_item *temp = realloc(*archive, (*archive_size + 1) * sizeof(Directory_item));
                if (temp != NULL) *archive = temp;
                else {
                    result = MALLOC_ERROR;
                    current_item = subdir;
                    break;
                }
                (*archive_size)++;
                (*archive)[(*current_index)++] = subdir;
                long subdir_size = archive_directory(newpath, archive, current_index, archive_size);
                if (subdir_size < 0) {
                    result = subdir_size;
                    break;
                }
                dir_size += subdir_size;
            } 
            else if (S_ISREG(st.st_mode)) {
                Directory_item file = {0};
                file.is_dir = false;
                file.file_path = strdup(newpath);
                if (file.file_path == NULL) {
                    result = MALLOC_ERROR;
                    break;
                }
                file.file_size = read_raw(newpath, &file.file_data);
                if (file.file_size < 0) {
                    result = FILE_READ_ERROR;
                    current_item = file;
                    break;
                }
                dir_size += file.file_size;
                Directory_item *temp = realloc(*archive, (*archive_size + 1) * sizeof(Directory_item));
                if (temp != NULL) *archive = temp;
                else {
                    result = MALLOC_ERROR;
                    current_item = file;
                    break;
                }

                (*archive_size)++;
                (*archive)[(*current_index)++] = file;
            }
            free(newpath);
            newpath = NULL;
        }
        
        if (result < 0) break;
        result = dir_size;
        break;
    }
    
    if (result < 0) {
        if (current_item.is_dir) {
            free(current_item.dir_path);
        } else {
            free(current_item.file_path);
            free(current_item.file_data);
        }
    }
    free(newpath);
    if (directory != NULL) closedir(directory);
    return result;
}

/*
 * Szerializalja az archivalt mappat egy bufferbe.
 * Siker eseten a buffer meretet adja vissza, hiba eseten negativ kodot.
 */
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


/*
 * Kicsomagolja az archivalt mappat a megadott utvonalra, letrehozza a mappakat es fajlokat.
 * Siker eseten 0-t ad vissza, hiba eseten negativ kodot.
 */
int extract_directory(char *path, Directory_item *archive, int archive_size, bool force) {
    if (path == NULL) path = ".";
    for (int current_index = 0; current_index < archive_size; current_index++) {
        Directory_item *current = &archive[current_index];
        char *item_path = current->is_dir ? current->dir_path : current->file_path;
        char *full_path = malloc(strlen(path) + strlen(item_path) + 2);
        if (full_path == NULL) return MALLOC_ERROR;
        /* Ha a felhasznalo adott kimeneti mappat, akkor ott ujrakezdjuk a mappa felepiteset. */
        strcpy(full_path, path);
        strcat(full_path, "/");
        strcat(full_path, item_path);
        if (current->is_dir) {
           int ret = mkdir(full_path, 0755);
           if (ret != 0 && errno != EEXIST) {
               free(full_path);
               return MKDIR_ERROR;
           }
        }
        else {
            int ret = write_raw(full_path, current->file_data, current->file_size, force);
            if (ret < 0) {
                free(full_path);
                return FILE_WRITE_ERROR;
            }
        }
        free(full_path);
    }
    return SUCCESS;
}

/*
 * Visszaalakitja a szerializalt bufferbol az archivum tombot.
 * Siker eseten az archivum meretet adja vissza, hiba eseten negativ kodot.
 */
int deserialize_archive(Directory_item **archive, char *buffer) {
    int archive_size;
    char *current = buffer;
    memcpy(&archive_size, current, sizeof(int));
    current += sizeof(int);
    *archive = calloc(archive_size, sizeof(Directory_item));
    if (*archive == NULL) return MALLOC_ERROR;
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

/*
 * Tomoriteshez szukseges mappa feldolgozas.
 * Bejarja a mappat, archivalja es szerializalja az adatokat.
 * Sikeres muveletek eseten a szerializalt adat hosszat adja vissza, hiba eseten negativ erteket.
 */
int prepare_directory(char *input_file, char **data, int *directory_size) {
    char current_path[1000];
    char *sep = strrchr(input_file, '/');
    char *parent_dir = NULL;
    char *file_name = NULL;
    Directory_item *archive = NULL;
    int archive_size = 0;
    int current_index = 0;
    int res = 0;
    
    while (true) {
        if (getcwd(current_path, sizeof(current_path)) == NULL) {
            printf("Nem sikerult elmenteni az utat.\n");
            res = DIRECTORY_ERROR;
            break;
        }
        

        /* Kulso eleresi ut eseteten athelyezkedunk a szulo mappaba, hogy a tarolt utak relativak maradjanak. */
        if (sep != NULL && !(strncmp(input_file, "./", 2) == 0 || strncmp(input_file, "../", 3) == 0)) {
            if (sep == input_file) {
                parent_dir = strdup("/");
                if (parent_dir == NULL) {
                    res = MALLOC_ERROR;
                    break;
                }
            }
            else {
                int parent_dir_len = sep - input_file;
                parent_dir = malloc(parent_dir_len + 1);
                if (parent_dir == NULL) {
                    res = MALLOC_ERROR;
                    break;
                }
                strncpy(parent_dir, input_file, parent_dir_len);
                parent_dir[parent_dir_len] = '\0';
            }
            file_name = strdup(sep + 1);
            if (file_name == NULL) {
                res = MALLOC_ERROR;
                break;
            }
            if (chdir(parent_dir) != 0) {
                printf("Nem sikerult belepni a mappaba.\n");
                res = DIRECTORY_ERROR;
                break;
            }
        }
        
        *directory_size = archive_directory((file_name != NULL) ? file_name : input_file, &archive, &current_index, &archive_size);
        if (*directory_size < 0) {
            if (*directory_size == MALLOC_ERROR) {
                printf("Nem sikerult lefoglalni a memoriat a mappa archivallasakor.\n");
            } else if (*directory_size == DIRECTORY_OPEN_ERROR) {
                printf("Nem sikerult megnyitni a mappat.\n");
            } else if (*directory_size == FILE_READ_ERROR) {
                printf("Nem sikerult beolvasni egy fajlt a mappabol.\n");
            } else {
                printf("Nem sikerult a mappa archivallasa.\n");
            }
            res = *directory_size;
            break;
        }
        
        res = serialize_archive(archive, archive_size, data);
        if (res < 0) {
            if (res == MALLOC_ERROR) {
                printf("Nem sikerult lefoglalni a memoriat a szerializalaskor.\n");
            } else if (res == EMPTY_DIRECTORY) {
                printf("A mappa ures.\n");
            } else {
                printf("Nem sikerult a mappa szerializalasa.\n");
            }
            break;
        }
        
        /* Visszalepunk az eredeti mappaba. */
        if (sep != NULL && !(strncmp(input_file, "./", 2) == 0 || strncmp(input_file, "../", 3) == 0)) {
            if (chdir(current_path) != 0) {
                printf("Nem sikerult kilepni a mappabol.\n");
                res = DIRECTORY_ERROR;
                if (*data != NULL) {
                    free(*data);
                    *data = NULL;
                }
                break;
            }
        }
        break;
    }
    
    if (archive_size > 0) {
        for (int i = 0; i < archive_size; ++i) {
            if (archive[i].is_dir) {
                free(archive[i].dir_path);
            } else {
                free(archive[i].file_path);
                free(archive[i].file_data);
            }
        }
    }
    free(archive);
    free(parent_dir);
    free(file_name);
    
    return res;
}

/*
 * Kitomoriteshez szukseges mappa feldolgozas.
 * Deszerializalja es kitomoriti az archivalt mappakat.
 * Sikeres muveletek eseten 0-t, hiba eseten negativ erteket ad vissza.
 */
int restore_directory(char *raw_data, char *output_file, bool force) {
    Directory_item *archive = NULL;
    int archive_size = 0;
    int res = 0;
    
    while (true) {
        archive_size = deserialize_archive(&archive, raw_data);
        if (archive_size < 0) {
            if (archive_size == MALLOC_ERROR) {
                printf("Nem sikerult lefoglalni a memoriat a beolvasaskor.\n");
            } else {
                printf("Nem sikerult a tomoritett mappa beolvasasa.\n");
            }
            res = archive_size;
            break;
        }
        
        if (output_file != NULL) {
            if (mkdir(output_file, 0755) != 0 && errno != EEXIST) {
                printf("Nem sikerult letrehozni a kimeneti mappat.\n");
                res = MKDIR_ERROR;
                break;
            }
        }
        
        int ret = extract_directory(output_file != NULL ? output_file : ".", archive, archive_size, force);
        if (ret != 0) {
            if (ret == MKDIR_ERROR) {
                printf("Nem sikerult letrehozni egy mappat a kitomoriteskor.\n");
            } else if (ret == FILE_WRITE_ERROR) {
                printf("Nem sikerult kiirni egy fajlt a kitomoriteskor.\n");
            } else {
                printf("Nem sikerult a mappa kitomoritese.\n");
            }
            res = ret;
            break;
        }
        break;
    }
    if (archive_size > 0) {
        for (int i = 0; i < archive_size; ++i) {
            if (archive[i].is_dir) {
                free(archive[i].dir_path);
            } else {
                free(archive[i].file_path);
                free(archive[i].file_data);
            }
        }
    }
    free(archive);
    
    return res;
}
