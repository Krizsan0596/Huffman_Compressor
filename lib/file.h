#ifndef FILE_H
#define FILE_H

#include <stdio_ext.h>
int read_raw(char file_name[], char* data);
void read_compressed(char file_name[], char *out_file, char *huffman_tree, char *compressed_data);
long get_file_size(FILE* f);

#endif
