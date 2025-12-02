#ifndef UTIL_H
#define UTIL_H

#include "data_types.h"

void print_usage(const char *prog_name);
int parse_arguments(int argc, char* argv[], Arguments *args);
char* generate_output_file(char *input_file);
int run_compression(Arguments args);
int run_decompression(Arguments args);

#endif
