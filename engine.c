#include "engine.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 256

int search_count(char *filename, char *target) {
    FILE *file;
    char *window = NULL;
    int window_length = 0;
    int target_length;
    int count = 0;
    int character;

    target_length = strlen(target);
    file = fopen(filename, "r");

    window = malloc(target_length);

    while ((character = fgetc(file)) != EOF) {
        window[window_length++] = (char)character;

        if (window_length == target_length) {
            if (memcmp(window, target, target_length) == 0) {
                count++;
            }
            memmove(window, window + 1, target_length - 1);
            window_length = target_length - 1;
        }
    }

    free(window);
    fclose(file);
    return count;
}

struct count_result search_instance(char *filename,char *target){
    struct count_result result;
    result.count = search_count(filename, target);
    return result;
}