#include "file.h"
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

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

int write_raw(char *file_name, char *data, long file_size){
    FILE* f;
    f = fopen(file_name, "r");
    if (f != NULL) { // Check if file exists
        fclose(f);
        printf("Letezik a fajl (%s). Felulirjam? [I/n]>", file_name);
        char input;
        scanf("%c", &input);
        if (tolower(input) != 'y') return 3;
    }
    f = fopen(file_name, "wb");
    if (f == NULL) return 1;
    fwrite(data, sizeof(char), file_size, f);
    if (file_size != get_file_size(f)) return 2;
    return 0;
}
int read_compressed(char file_name[], long out_size, char *out_file, char *huffman_tree, char *compressed_data); // Replace with tree struct.
int write_compressed(char file_name[], long file_size, long original_size, char *original_file, char *huffman_tree, char *compressed_data) {
    
}
