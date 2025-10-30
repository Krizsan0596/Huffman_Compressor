#ifndef FILE_H
#define FILE_H

#include <stdio_ext.h>
int read_raw(char file_name[], char* data);
int write_raw(char file_name[], char* data, long file_size);
int read_compressed(char file_name[], long out_size, char *out_file, char *huffman_tree, char *compressed_data);
int write_compressed(char file_name[], long file_size, long out_size, char *out_file, char *huffman_tree, char *compressed_data);
long get_file_size(FILE* f);

#endif
