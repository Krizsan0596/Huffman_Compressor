#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "data_types.h"

int archive_directory(char *path, Directory_item **archive, int *current, int *archive_size);
int extract_directory(char *path, Directory_item **archive, int archive_size, bool force);

#endif // DIRECTORY_H
