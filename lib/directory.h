#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "data_types.h"
#include <stdio.h>

long archive_directory(char *path, Directory_item **archive, int *current, int *archive_size);
long serialize_archive(Directory_item *item, FILE *output);
int deserialize_archive(Directory_item **archive, char *buffer);
int extract_directory(char *path, Directory_item *archive, int archive_size, bool force, bool no_preserve_perms);
int prepare_directory(char *input_file, char **data, int *directory_size);
int restore_directory(char *raw_data, char *output_file, bool force, bool no_preserve_perms);

#endif // DIRECTORY_H
