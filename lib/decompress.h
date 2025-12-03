#ifndef DECOMPRESS_H
#define DECOMPRESS_H

#include "data_types.h"

int decompress(Compressed_file *compressed, char *raw);
// All output pointers must be valid, caller-owned, non-NULL pointers.
int run_decompression(Arguments args, char **raw_data, long *raw_size, bool *is_directory, char **original_name);

#endif
