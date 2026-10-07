#include "engine.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <omp.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <sys/types.h>

#define MAX_LINE_LENGTH 256

struct instance_chunk {
    char **instances;
    int count;
    size_t capacity;
    int failed;
};

static int append_instance(struct instance_chunk *chunk, const char *line,
                           size_t length) {
    char **instances;
    char *instance;

    if (chunk->count == INT_MAX) {
        return 0;
    }
    if ((size_t)chunk->count == chunk->capacity) {
        size_t new_capacity = chunk->capacity == 0 ? 8 : chunk->capacity * 2;

        if (new_capacity < chunk->capacity ||
            new_capacity > SIZE_MAX / sizeof(*instances)) {
            return 0;
        }
        instances = realloc(chunk->instances, new_capacity * sizeof(*instances));
        if (instances == NULL) {
            return 0;
        }
        chunk->instances = instances;
        chunk->capacity = new_capacity;
    }

    if (length == SIZE_MAX) {
        return 0;
    }
    instance = malloc(length + 1);
    if (instance == NULL) {
        return 0;
    }
    memcpy(instance, line, length);
    instance[length] = '\0';
    chunk->instances[chunk->count++] = instance;
    return 1;
}

static void free_instance_chunks(struct instance_chunk *chunks, int count) {
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < chunks[i].count; j++) {
            free(chunks[i].instances[j]);
        }
        free(chunks[i].instances);
    }
}

int search_count(char *filename, char *target) {
    struct stat file_stat;
    long long file_size;
    size_t target_length;
    int thread_count;
    int total = 0;
    int failed = 0;

    if (filename == NULL || target == NULL || target[0] == '\0') {
        return 0;
    }
    if (stat(filename, &file_stat) != 0 || file_stat.st_size < 0) {
        fprintf(stderr, "%s: unable to determine file size\n", filename);
        return 0;
    }
    if (file_stat.st_size > LLONG_MAX) {
        fprintf(stderr, "%s: file is too large to search\n", filename);
        return 0;
    }

    file_size = (long long)file_stat.st_size;
    target_length = strlen(target);
    if (file_size == 0 || target_length > (size_t)file_size) {
        return 0;
    }

    thread_count = omp_get_max_threads();
    if ((long long)thread_count > file_size) {
        thread_count = (int)file_size;
    }

    #pragma omp parallel for num_threads(thread_count) reduction(+:total) reduction(|:failed) schedule(static)
    for (int chunk = 0; chunk < thread_count; chunk++) {
        long long base_size = file_size / thread_count;
        long long remainder = file_size % thread_count;
        long long start = base_size * chunk +
                          (remainder * chunk) / thread_count;
        long long end = base_size * (chunk + 1) +
                        (remainder * (chunk + 1)) / thread_count;
        long long chunk_length = end - start;
        long long bytes_remaining = file_size - start;
        size_t bytes_to_read;
        size_t overlap;
        size_t read_length;
        char *buffer;
        FILE *file = fopen(filename, "rb");

        if (file == NULL || fseeko(file, (off_t)start, SEEK_SET) != 0) {
            failed = 1;
            if (file != NULL) {
                fclose(file);
            }
            continue;
        }

        bytes_to_read = (size_t)chunk_length;
        overlap = target_length - 1;
        if ((long long)overlap > bytes_remaining - chunk_length) {
            overlap = (size_t)(bytes_remaining - chunk_length);
        }
        read_length = bytes_to_read + overlap;
        buffer = malloc(read_length);
        if (buffer == NULL) {
            failed = 1;
            fclose(file);
            continue;
        }

        if (fread(buffer, 1, read_length, file) != read_length) {
            failed = 1;
            free(buffer);
            fclose(file);
            continue;
        }
        fclose(file);

        for (size_t i = 0; i < bytes_to_read; i++) {
            if (target_length <= read_length - i &&
                memcmp(buffer + i, target, target_length) == 0) {
                total++;
            }
        }
        free(buffer);
    }

    if (failed) {
        fprintf(stderr, "%s: failed to read file while searching\n", filename);
        return 0;
    }
    return total;
}


struct count_result search_instance(char *filename, char *target) {
    struct count_result result = {0, NULL};
    struct stat file_stat;
    long long file_size;
    struct instance_chunk *chunks;
    int thread_count;
    int failed = 0;
    size_t total = 0;

    if (filename == NULL || target == NULL || target[0] == '\0') {
        return result;
    }
    if (stat(filename, &file_stat) != 0 || file_stat.st_size < 0) {
        fprintf(stderr, "%s: unable to determine file size\n", filename);
        return result;
    }
    if (file_stat.st_size > LLONG_MAX) {
        fprintf(stderr, "%s: file is too large to search\n", filename);
        return result;
    }

    file_size = (long long)file_stat.st_size;
    if (file_size == 0) {
        return result;
    }

    thread_count = omp_get_max_threads();
    if ((long long)thread_count > file_size) {
        thread_count = (int)file_size;
    }
    chunks = calloc((size_t)thread_count, sizeof(*chunks));
    if (chunks == NULL) {
        fprintf(stderr, "%s: unable to allocate search results\n", filename);
        return result;
    }

    #pragma omp parallel for num_threads(thread_count) schedule(static)
    for (int chunk_index = 0; chunk_index < thread_count; chunk_index++) {
        long long base_size = file_size / thread_count;
        long long remainder = file_size % thread_count;
        long long start = base_size * chunk_index +
                          (remainder * chunk_index) / thread_count;
        long long end = base_size * (chunk_index + 1) +
                        (remainder * (chunk_index + 1)) / thread_count;
        struct instance_chunk *chunk = &chunks[chunk_index];
        FILE *file = fopen(filename, "r");
        char *line = NULL;
        size_t line_capacity = 0;
        ssize_t line_length;
        off_t line_start;

        if (file == NULL || fseeko(file, (off_t)start, SEEK_SET) != 0) {
            chunk->failed = 1;
            if (file != NULL) {
                fclose(file);
            }
            continue;
        }

        if (start > 0) {
            int previous;

            if (fseeko(file, (off_t)(start - 1), SEEK_SET) != 0) {
                chunk->failed = 1;
                fclose(file);
                continue;
            }
            previous = fgetc(file);
            if (previous != '\n') {
                if (getline(&line, &line_capacity, file) < 0 && ferror(file)) {
                    chunk->failed = 1;
                }
            }
        }

        while (!chunk->failed) {
            char *start_of_text;
            char *end_of_text;
            char *match;
            size_t instance_length;
            size_t target_length = strlen(target);

            line_start = ftello(file);
            if (line_start < 0) {
                chunk->failed = 1;
                break;
            }
            if ((long long)line_start >= end) {
                break;
            }
            line_length = getline(&line, &line_capacity, file);
            if (line_length < 0) {
                if (ferror(file)) {
                    chunk->failed = 1;
                }
                break;
            }
            start_of_text = line;
            end_of_text = line + line_length;

            while (start_of_text < end_of_text &&
                   isspace((unsigned char)*start_of_text)) {
                start_of_text++;
            }
            while (end_of_text > start_of_text &&
                   isspace((unsigned char)end_of_text[-1])) {
                end_of_text--;
            }
            instance_length = (size_t)(end_of_text - start_of_text);

            match = line;
            while ((match = strstr(match, target)) != NULL) {
                if (!append_instance(chunk, start_of_text, instance_length)) {
                    chunk->failed = 1;
                    break;
                }
                match += target_length;
            }
        }
        if (ferror(file)) {
            chunk->failed = 1;
        }
        free(line);
        fclose(file);
    }

    for (int i = 0; i < thread_count; i++) {
        if (chunks[i].failed) {
            failed = 1;
        }
        if ((size_t)chunks[i].count > (size_t)INT_MAX - total) {
            failed = 1;
        } else {
            total += (size_t)chunks[i].count;
        }
    }
    if (failed) {
        fprintf(stderr, "%s: failed to read or store search results\n", filename);
        free_instance_chunks(chunks, thread_count);
        free(chunks);
        return result;
    }

    if (total > SIZE_MAX / sizeof(*result.instances)) {
        fprintf(stderr, "%s: too many search results to store\n", filename);
        free_instance_chunks(chunks, thread_count);
        free(chunks);
        return result;
    }
    if (total > 0) {
        result.instances = malloc(total * sizeof(*result.instances));
        if (result.instances == NULL) {
            fprintf(stderr, "%s: unable to allocate search results\n", filename);
            free_instance_chunks(chunks, thread_count);
            free(chunks);
            return result;
        }
        for (int i = 0; i < thread_count; i++) {
            if (chunks[i].count > 0) {
                memcpy(result.instances + result.count, chunks[i].instances,
                       (size_t)chunks[i].count * sizeof(*result.instances));
            }
            result.count += chunks[i].count;
            free(chunks[i].instances);
        }
    }
    free(chunks);
    return result;
}
