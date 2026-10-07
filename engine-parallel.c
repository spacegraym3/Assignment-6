#define _POSIX_C_SOURCE 200809L

#include "engine.h"
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_WORKERS 10

struct worker_args {
    char *filename;
    char *target;
    long start;
    long end;
    int count;
    struct count_result result;
    size_t capacity;
    int failed;
};

static int worker_count(long file_size) {
    int count = MAX_WORKERS;

    if (file_size <= 0) {
        return 1;
    }
    if (file_size < count) {
        count = (int)file_size;
    }
    return count;
}

static long long get_file_size(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return (long long)st.st_size;
    }
    return -1; // Error opening or finding file
}

static void set_worker_ranges(struct worker_args *args, int count,
                              char *filename, char *target, long file_size) {
    long chunk_size = file_size / count;
    long remainder = file_size % count;
    int i;

    for (i = 0; i < count; i++) {
        args[i].filename = filename;
        args[i].target = target;
        args[i].start = chunk_size * i + remainder * i / count;
        args[i].end = chunk_size * (i + 1) + remainder * (i + 1) / count;
        args[i].result.count = 0;
        args[i].result.instances = NULL;
        args[i].capacity = 0;
        args[i].count = 0;
        args[i].failed = 0;
    }
}

static int run_workers(struct worker_args *args, pthread_t *threads,
                       int count, void *(*worker)(void *)) {
    int started[MAX_WORKERS] = {0};
    int success = 1;

    for (int i = 0; i < count; i++) {
        pthread_create(&threads[i], NULL, worker, &args[i]);
        started[i] = 1;
    }

    for (int i = 0; i < count; i++) {

        if (!started[i]) {
            continue;
        }
        pthread_join(threads[i], NULL);
    }
    return success;
}

static void *count_worker(void *arg) {
    struct worker_args *args = arg;
    FILE *file = fopen(args->filename, "rb");
    int target_length = strlen(args->target);
    int chunk_length = (int)(args->end - args->start);
    char *buffer;

    int read_length = chunk_length + target_length - 1;
    buffer = malloc(read_length == 0 ? 1 : read_length);
    if (fseek(file, args->start, SEEK_SET) != 0) {
        args->failed = 1;
        free(buffer);
        fclose(file);
        return NULL;
    }
    int bytes_read = fread(buffer, 1, read_length, file);

    for (int i = 0; i < chunk_length && i + target_length <= bytes_read; i++) {
        if (memcmp(buffer + i, args->target, target_length) == 0) {
            args->count++;
        }
    }

    free(buffer);
    fclose(file);
    return NULL;
}

static int append_instance(struct worker_args *args, const char *line,
                           size_t line_length) {
    char **instances;
    char *instance;

    if ((size_t)args->result.count == args->capacity) {
        size_t new_capacity = args->capacity == 0 ? 8 : args->capacity * 2;

        if (new_capacity < args->capacity ||
            new_capacity > SIZE_MAX / sizeof(*instances)) {
            return 0;
        }
        instances = realloc(args->result.instances,
                            new_capacity * sizeof(*instances));
        if (instances == NULL) {
            return 0;
        }
        args->result.instances = instances;
        args->capacity = new_capacity;
    }

    instance = malloc(line_length + 1);
    if (instance == NULL) {
        return 0;
    }
    memcpy(instance, line, line_length);
    instance[line_length] = '\0';
    args->result.instances[args->result.count++] = instance;
    return 1;
}

static void *instance_worker(void *arg) {
    struct worker_args *args = arg;
    FILE *file = fopen(args->filename, "r");
    char *line = NULL;
    size_t line_capacity = 0;
    long line_start;
    ssize_t line_length;
    if (fseek(file, args->start, SEEK_SET) != 0) {
        args->failed = 1;
        fclose(file);
        return NULL;
    }

    if (args->start > 0) {
        int previous;

        if (fseek(file, args->start - 1, SEEK_SET) != 0) {
            args->failed = 1;
            fclose(file);
            return NULL;
        }
        previous = fgetc(file);
        if (previous != '\n' && getline(&line, &line_capacity, file) < 0 &&
            ferror(file)) {
            args->failed = 1;
            free(line);
            fclose(file);
            return NULL;
        }
    }

    while ((line_start = ftell(file)) >= 0 && line_start < args->end &&
           (line_length = getline(&line, &line_capacity, file)) >= 0) {
        char *start = line;
        char *end = line + line_length;
        char *match;
        size_t instance_length;
        size_t target_length = strlen(args->target);

        while (start < end && isspace((unsigned char)*start)) {
            start++;
        }
        while (end > start && isspace((unsigned char)end[-1])) {
            end--;
        }
        instance_length = (size_t)(end - start);

        match = line;
        while ((match = strstr(match, args->target)) != NULL) {
            if (!append_instance(args, start, instance_length)) {
                args->failed = 1;
                free(line);
                fclose(file);
                return NULL;
            }
            match += target_length;
        }
    }

    free(line);
    fclose(file);
    return NULL;
}

int search_count(char *filename, char *target) {
    struct worker_args args[MAX_WORKERS];
    pthread_t threads[MAX_WORKERS] = {0};
    int count;
    int i;
    int total = 0;

    long file_size = get_file_size(filename);
    count = worker_count(file_size);
    set_worker_ranges(args, count, filename, target, file_size);
    if (!run_workers(args, threads, count, count_worker)) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        total += args[i].count;
    }
    return total;
}

struct count_result search_instance(char *filename, char *target) {
    struct worker_args args[MAX_WORKERS];
    pthread_t threads[MAX_WORKERS] = {0};
    struct count_result result = {0, NULL};
    int count;
    int i;
    int total = 0;
    int offset = 0;

    long file_size = get_file_size(filename);
    count = worker_count(file_size);
    set_worker_ranges(args, count, filename, target, file_size);
    if (!run_workers(args, threads, count, instance_worker)) {
        return result;
    }

    for (i = 0; i < count; i++) {
        if (args[i].failed) {
            fprintf(stderr, "%s: failed to read or store instances\n", filename);
            for (int j = 0; j < count; j++) {
                for (int k = 0; k < args[j].result.count; k++) {
                    free(args[j].result.instances[k]);
                }
                free(args[j].result.instances);
            }
            return result;
        }
        total += args[i].result.count;
    }

    if (total == 0) {
        return result;
    }
    result.instances = malloc((size_t)total * sizeof(*result.instances));
    if (result.instances == NULL) {
        fprintf(stderr, "Unable to allocate instance result\n");
        for (i = 0; i < count; i++) {
            for (int j = 0; j < args[i].result.count; j++) {
                free(args[i].result.instances[j]);
            }
            free(args[i].result.instances);
        }
        return result;
    }

    for (i = 0; i < count; i++) {
        int j;

        for (j = 0; j < args[i].result.count; j++) {
            result.instances[offset++] = args[i].result.instances[j];
        }
        free(args[i].result.instances);
    }
    result.count = total;
    return result;
}
