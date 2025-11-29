#ifndef DECOMPRESS_H
#define DECOMPRESS_H

#include "data_types.h"

int decompress(Compressed_file *compressed, char *raw);
int run_decompression(Arguments args);

#endif
