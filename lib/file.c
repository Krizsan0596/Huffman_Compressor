#include "file.h"
#include "data_types.h"
#include <ctype.h>
#include <iso646.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "debugmalloc.h"



long get_file_size(FILE *f){
    long current = ftell(f);
    if (fseek(f, 0, SEEK_END) != 0) return -1;
    long size = ftell(f);
    fseek(f, 0, current);
    return size;
} 

int read_raw(char file_name[], char** data){
    FILE* f;
    f = fopen(file_name, "rb");
    if (f == NULL) return -1; // Not exists.
    long file_size = get_file_size(f);
    *data = (char*)malloc(file_size);
    if (*data == NULL) {
        fclose(f);
        return -3; // malloc error
    }
    size_t read_size = fread(*data, sizeof(char), file_size, f);
    fclose(f);
    if (read_size != file_size) {
        free(*data);
        return -2;
    }
    return read_size;
}

int write_raw(char *file_name, char *data, long file_size, bool overwrite){
    FILE* f;
    f = fopen(file_name, "r");
    if (f != NULL) { 
        if (!overwrite) {
            fclose(f);
            printf("Letezik a fajl (%s). Felulirjam? [I/n]>", file_name);
            char input;
            if (scanf(" %c", &input) != 1) return 4; 
            if (tolower(input) != 'y') return 3;
        }
        else fclose(f);
    }
    f = fopen(file_name, "wb");
    if (f == NULL) return 1;
    fwrite(data, sizeof(char), file_size, f);
    fclose(f);
    f = fopen(file_name, "rb");
    if (f == NULL) return 1;
    long written_size = get_file_size(f);
    fclose(f);
    if (file_size != written_size) return 2;
    return 0;
}
int read_compressed(char file_name[], Compressed_file *compressed){
    int ret = 0;
    FILE* f = fopen(file_name, "rb");
    if (f == NULL) {
        return 1; 
    }

    compressed->original_file = NULL;
    compressed->huffman_tree = NULL;
    compressed->compressed_data = NULL;
    compressed->file_name = NULL;

    while (true) {
        if (fread(compressed->magic, sizeof(char), sizeof(magic), f) != sizeof(magic)) {
            ret = 2; // File read error
            break;
        }

        if (memcmp(compressed->magic, magic, sizeof(magic)) != 0) {
            ret = 3; // Magic error
            break;
        }

        if (fread(&compressed->original_size, sizeof(long), 1, f) != 1) {
            ret = 2;
            break;
        }

        long name_len = 0;
        if (fread(&name_len, sizeof(long), 1, f) != 1) {
            ret = 2;
            break;
        }
        if (name_len < 0) {
            ret = 3;
            break;
        }

        compressed->original_file = (char*)malloc(name_len + 1);
        if (compressed->original_file == NULL) {
            ret = -1; // Malloc error
            break;
        }
        if ((long)fread(compressed->original_file, sizeof(char), name_len, f) != name_len) {
            ret = 2;
            break;
        }
        compressed->original_file[name_len] = '\0';

        if (fread(&compressed->tree_size, sizeof(long), 1, f) != 1) {
            ret = 2;
            break;
        }
        if (compressed->tree_size < 0) {
            ret = 3;
            break;
        }

        compressed->huffman_tree = (Node*)malloc(compressed->tree_size);
        if (compressed->huffman_tree == NULL) {
            ret = -1;
            break;
        }
        if ((long)fread(compressed->huffman_tree, sizeof(char), compressed->tree_size, f) != compressed->tree_size) {
            ret = 2;
            break;
        }

        if (fread(&compressed->data_size, sizeof(long), 1, f) != 1) {
            ret = 2;
            break;
        }
        if (compressed->data_size < 0) {
            ret = 3;
            break;
        }

        long compressed_bytes = (long)ceil((double)compressed->data_size / 8.0);
        compressed->compressed_data = (char*)malloc(compressed_bytes * sizeof(char));
        if (compressed->compressed_data == NULL) {
            ret = -1;
            break;
        }
        if ((long)fread(compressed->compressed_data, sizeof(char), compressed_bytes, f) != compressed_bytes) {
            ret = 2;
            break;
        }

        compressed->file_name = strdup(file_name);
        if (compressed->file_name == NULL) {
            ret = -1;
            break;
        }

        break;
    }

    fclose(f);

    if (ret != 0) {
        free(compressed->original_file);
        free(compressed->huffman_tree);
        free(compressed->compressed_data);
        free(compressed->file_name);
        compressed->original_file = NULL;
        compressed->huffman_tree = NULL;
        compressed->compressed_data = NULL;
        compressed->file_name = NULL;
    }

    return ret;
}
int write_compressed(Compressed_file *compressed, bool overwrite) {
    long name_len = strlen(compressed->original_file);
    long file_size = (sizeof(char) * 4) + sizeof(long) + sizeof(long) + name_len * sizeof(char) + sizeof(long) + compressed->tree_size + sizeof(long) + ceil((float)compressed->data_size/8);
    char *data = malloc(file_size);
    char *current = &data[0];
    for (int i = 0; i < 4; i++) {
        data[i] = magic[i];
    }
    current += 4;
    memcpy(current, &compressed->original_size, sizeof(long));
    current += sizeof(long);
    memcpy(current, &name_len, sizeof(long));
    current += sizeof(long);
    memcpy(current, compressed->original_file, name_len);
    current += name_len;
    memcpy(current, &compressed->tree_size, sizeof(long));
    current += sizeof(long);
    memcpy(current, compressed->huffman_tree, compressed->tree_size);
    current += compressed->tree_size;
    memcpy(current, &compressed->data_size, sizeof(long));
    current += sizeof(long);
    memcpy(current, compressed->compressed_data, ceil((float)compressed->data_size/8));
    int res = write_raw(compressed->file_name, data, file_size, overwrite);
    free(data);
    return res;
}
