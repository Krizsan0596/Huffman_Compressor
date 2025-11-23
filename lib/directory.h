#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "data_types.h"

int archive_directory(char *path, Directory_item **archive, int *current, int *archive_size);
long serialize_archive(Directory_item *archive, int archive_size, char **buffer);
int deserialize_archive(Directory_item **archive, char *buffer);
int extract_directory(char *path, Directory_item **archive, int archive_size, bool force);

#endif // DIRECTORY_H
