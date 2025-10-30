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

int read_raw(char file_name[], char* data){
    FILE* f;
    f = fopen(file_name, "rb");
    if (f == NULL) return 1; // Not exists.
    long file_size = get_file_size(f);
    data = (char*)malloc(file_size);
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
            scanf(" %c", &input);
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
int read_compressed(char file_name[], long out_size, char *out_file, char *huffman_tree, char *compressed_data);
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
