# Change summary: Adds functionality to archive and extract entire directories, introducing directory traversal, serialization, and deserialization logic.

## File: lib/directory.c
### L24: [HIGH] Inefficient memory allocation using realloc in a loop.

Calling `realloc` for every single item added to the archive is highly inefficient. In the worst case, this can lead to `O(n^2)` performance as `realloc` may have to copy the entire growing data block to a new memory location on each call. A better approach is to use an exponential growth strategy (e.g., doubling the capacity when the current capacity is reached) to reduce the number of reallocations.

This issue also occurs on L51 for files.

### L35: [CRITICAL] Potential memory leak on recursive call failure.

If the recursive call to `archive_directory` returns an error, the function returns immediately. However, the `newpath` buffer, which was allocated at the start of the loop (L17), is not freed in this error path, leading to a memory leak.

Suggested change:
```
            memcpy(&(*archive)[(*current_index)++], &subdir, sizeof(Directory_item));
            int ret = archive_directory(newpath, archive, current_index, archive_size);
            if (ret != 0) {
-               return ret;
+               free(newpath);
+               return ret;
            }
        }
        else if (dir->d_type == DT_REG) {
```

### L44: [CRITICAL] Memory leak when `read_raw` fails.

If `read_raw` returns an error (a negative value), the function returns -3. However, `file.file_path`, which was allocated with `strdup` on L42, is not freed before returning. This causes a memory leak for every file that fails to be read.

Suggested change:
```
            file.file_path = strdup(newpath);
            if (file.file_path == NULL) return -1;
            file.file_size = read_raw(newpath, &file.file_data);
-           if (file.file_size < 0) return -3;
+           if (file.file_size < 0) {
+               free(file.file_path);
+               return -3;
+           }
            Directory_item *temp = realloc(*archive, (*archive_size + 1) * sizeof(Directory_item));
            if (temp != NULL) *archive = temp;
            else {
```

### L70: [MEDIUM] Empty directory is incorrectly treated as an error.

The function returns an error (`-2`) if the archive is empty (`archive_size == 0`). An empty directory is a valid input, and the function should handle this gracefully, likely by producing a zero-size buffer, rather than failing.

Suggested change:
```
int serialize_archive(Directory_item *archive, int archive_size, char **buffer) {
-   if (archive_size == 0) return -2; // empty dir
+   if (archive_size == 0) {
+       *buffer = NULL;
+       return 0;
+   }
    int data_size = 0;
    for (int i = 0; i < archive_size; i++) {
        data_size += sizeof(bool);
```

### L78: [MEDIUM] Use of non-portable types in serialized format.

The code uses `sizeof(bool)` and `sizeof(long)` (also on L88 and L94) to serialize data. The sizes of these types are not guaranteed to be the same across different architectures (e.g., 32-bit vs. 64-bit) or compilers, which makes the resulting archive file format non-portable. It is best practice to use fixed-size integer types from `<stdint.h>` (e.g., `uint8_t` for the boolean flag, and `uint64_t` for sizes and lengths) to ensure a consistent format.