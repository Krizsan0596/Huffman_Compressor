#ifndef FILE_H
#define FILE_H
#include "data_types.h"
#include <stdio_ext.h>
#include <stdbool.h>

int read_raw(char file_name[], char* data);
int write_raw(char file_name[], char* data, long file_size, bool overwrite);
int read_compressed(char file_name[], long out_size, char *out_file, char *huffman_tree, char *compressed_data);
int write_compressed(compressed_file *compressed, bool overwrite); 
long get_file_size(FILE* f);

#endif
