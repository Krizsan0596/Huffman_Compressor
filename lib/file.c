#include "file.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

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
    if (f == NULL) return 1;
    long file_size = get_file_size(f);
    data = (char*)malloc(file_size);
    size_t read_size = fread(data, 1, file_size, f);
    if (read_size != file_size) return 2;
    fclose(f);
    return 0;
}
