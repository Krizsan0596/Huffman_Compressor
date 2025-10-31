#include <stdio.h>
#include "../lib/file.h"
#include "../lib/debugmalloc.h"

int main(int argc, char* argv[]){
    // for (int i = 0; i < argc; i++){
    //     if (strcmp(argv[i], "-x") == 0) printf("Extract");
    //     else if (strcmp(argv[i], "-h") == 0) printf("Usage");
    //     else printf("Invalid");
    // }
    char *data;
    int res = read_raw("test.txt", data);
    long *frequencies = malloc(256 * sizeof(long));
    if (res == 1) {
        printf("File not found.");
        return 1;
    }
    else if (res == 2){
        printf("Reading file failed");
        return 1;
    }
    printf("%s", data);
    for (int i = 0; i < 256; i++){
        printf("\n%ld", frequencies[i]);
    }
    return 0;
}
