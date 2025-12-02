#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../lib/data_types.h"
#include "../lib/debugmalloc.h"
#include "../lib/util.h"

/*
 * A kapcsolokat feldolgozva elinditja a tomorites vagy kitomorites folyamatat, es a hibakodot adja vissza.
 */
int main(int argc, char* argv[]){
    Arguments args;
    int parse_result = parse_arguments(argc, argv, &args);
    if (parse_result == HELP_REQUESTED) {
        return SUCCESS;
    }
    if (parse_result == FILE_READ_ERROR) {
        return ENOENT;
    }
    if (parse_result != SUCCESS) {
        return parse_result;
    }

    /* Ellenorizzuk, hogy az -r valoban mappat jelol, vagy hibasan lett megadva. */
    if (args.directory) {
        struct stat st;
        int ret = stat(args.input_file, &st);
        if (ret != 0) {
            printf("Nem sikerult ellenorizni a mappat.\n");
            return ret;
        }
        else if (S_ISREG(st.st_mode)) args.directory = false;
    }
    else {
        struct stat st;
        int ret = stat(args.input_file, &st);
        if (ret != 0) {
            printf("Nem sikerult ellenorizni a fajlt.\n");
            return ret;
        }
        else if (S_ISDIR(st.st_mode)) {
            printf("Az -r kapcsolo nelkul nem tomorit mappat a program.\n");
            print_usage(argv[0]);
            return EISDIR;
        }
    }
    
    if (args.compress_mode) {
        return run_compression(args);
    } else if (args.extract_mode) {
        return run_decompression(args);
    } 
    else {
        printf("Az egyik modot (-c vagy -x) meg kell adni.\n");
        print_usage(argv[0]);
        return EINVAL;
    }
}
