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
    if (f == NULL) return 1; // Not exists.
    long file_size = get_file_size(f);
    *data = (char*)malloc(file_size);
    size_t read_size = fread(data, sizeof(char), file_size/sizeof(char), f);
    fclose(f);
    if (read_size != file_size) return 2;
    return 0;
}

int write_raw(char *file_name, char *data, long file_size, bool overwrite){
    FILE* f;
    f = fopen(file_name, "r");
    if (f != NULL) { // Check if file exists
        if (!overwrite) {
            fclose(f);
            printf("Letezik a fajl (%s). Felulirjam? [I/n]>", file_name);
            char input;
            if (scanf(" %c", &input) != 1) return 4; // Error reading user input
            if (tolower(input) != 'y') return 3;
        }
        else fclose(f);
    }
    f = fopen(file_name, "wb");
    if (f == NULL) return 1;
    fwrite(data, sizeof(char), file_size, f);
    long written_size = get_file_size(f);
    fclose(f);
    if (file_size != written_size) return 2;
    return 0;
}
int read_compressed(char file_name[], compressed_file *compressed){
    int ret = 0;
    char *data = NULL;
    FILE* f = fopen(file_name, "rb");
    if (f == NULL) {
        return 1; // File open error
    }

    compressed->original_file = NULL;
    compressed->huffman_tree = NULL;
    compressed->compressed_data = NULL;
    compressed->file_name = NULL;

    long file_size = get_file_size(f);
    if (file_size < 0) {
        fclose(f);
        return 1;
    }

    data = malloc(file_size);
    if (data == NULL) {
        fclose(f);
        return -1; // Malloc error
    }

    if (fread(data, 1, file_size, f) != file_size) {
        free(data);
        fclose(f);
        return 2; // File read error
    }
    
    fclose(f);

    char *current = data;
    long name_len = 0;

    while (true) {
        memcpy(&compressed->original_size, current, sizeof(long));
        current += sizeof(long);

        memcpy(&name_len, current, sizeof(long));
        current += sizeof(long);

        compressed->original_file = (char*)malloc(name_len + 1);
        if (compressed->original_file == NULL) { ret = -1; break; }
        memcpy(compressed->original_file, current, name_len);
        compressed->original_file[name_len] = '\0';
        current += name_len;

        memcpy(&compressed->tree_size, current, sizeof(long));
        current += sizeof(long);

        compressed->huffman_tree = (char*)malloc(compressed->tree_size);
        if (compressed->huffman_tree == NULL) { ret = -1; break; }
        memcpy(compressed->huffman_tree, current, compressed->tree_size);
        current += compressed->tree_size;

        memcpy(&compressed->data_size, current, sizeof(long));
        current += sizeof(long);

        long compressed_bytes = (long)ceil((double)compressed->data_size / 8.0);
        compressed->compressed_data = (char*)malloc(compressed_bytes);
        if (compressed->compressed_data == NULL) { ret = -1; break; }
        memcpy(compressed->compressed_data, current, compressed_bytes);
        current += compressed_bytes;

        compressed->file_name = strdup(file_name);
        if (compressed->file_name == NULL) { ret = -1; break; }

        break;
    }

    free(data);

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
int write_compressed(compressed_file *compressed, bool overwrite) {
    long name_len = strlen(compressed->original_file);
    long file_size = sizeof(long) + sizeof(long) + name_len * sizeof(char) + sizeof(long) + compressed->tree_size + sizeof(long) + ceil((float)compressed->data_size/8);
    char *data = malloc(file_size);
    char *current = &data[0];
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
