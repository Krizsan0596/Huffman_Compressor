#ifndef FILE_H
#define FILE_H
#include "data_types.h"
#include <stdio_ext.h>
#include <stdbool.h>

int read_raw(char file_name[], char* data);
int write_raw(char file_name[], char* data, long file_size, bool overwrite);
int read_compressed(char file_name[], compressed_file *compressed);
int write_compressed(compressed_file *compressed, bool overwrite); 
long get_file_size(FILE* f);

#endif
