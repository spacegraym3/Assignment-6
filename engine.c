#define _POSIX_C_SOURCE 200809L

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

struct count_result search_instance(char *filename, char *target) {
    FILE *file;
    struct count_result result = {0, NULL};
    char *line = NULL;
    size_t line_capacity = 0;
    ssize_t line_length;

    file = fopen(filename, "r");

    while ((line_length = getline(&line, &line_capacity, file)) >= 0) {
        char *start = line;
        char *end;
        char *match = line;
        char *instance;
        int instance_length;
        char **new_instances;
        int occurrences = 0;

        while (start < line + line_length &&
               isspace((unsigned char)*start)) {
            start++;
        }

        end = line + line_length;
        while (end > start && isspace((unsigned char)end[-1])) {
            end--;
        }

        instance_length = (int)(end - start);
        instance = malloc(instance_length + 1);
        if (instance == NULL) {
            break;
        }
        memcpy(instance, start, instance_length);
        instance[instance_length] = '\0';

        while ((match = strstr(match, target)) != NULL) {
            new_instances = realloc(result.instances,
                                    (int)(result.count + 1) * sizeof(*new_instances));
            if (new_instances == NULL) {
                free(instance);
                break;
            }

            result.instances = new_instances;
            result.instances[result.count++] = strdup(instance);
            if (result.instances[result.count - 1] == NULL) {
                free(instance);
                break;
            }
            occurrences++;
            match += strlen(target);
        }

        free(instance);
        if (occurrences == 0) {
            continue;
        }
        if (result.count == 0) {
            break;
        }
    }

    free(line);
    fclose(file);
    return result;
}