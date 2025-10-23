#include <stdio.h>
#include <string.h>
#include "../lib/file.h"


int main(int argc, char* argv[]){
    // for (int i = 0; i < argc; i++){
    //     if (strcmp(argv[i], "-x") == 0) printf("Extract");
    //     else if (strcmp(argv[i], "-h") == 0) printf("Usage");
    //     else printf("Invalid");
    // }
    char *data;
    int res = read_raw("Test file", data);
    if (res == 1) {
        printf("File not found.");
        return 1;
    }
    else if (res == 2){
        printf("Reading file failed");
        return 1;
    }
    printf("%s", *data);
    return 0;
}
