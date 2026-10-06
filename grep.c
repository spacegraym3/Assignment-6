#include "engine.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    // TODO: parse the arguments in argv.
    // You can expect argv[1] to be the mode
    // You can expect argv[2] to be the filepath
    // You can expect argv[3] to be the target word

    if (argc != 4) {
        fprintf(stderr,
                "Usage: %s <count|instance> <input_file> <target_word>\n",
                argv[0]);
        return 1;
    }

    // count data/warnpeace.txt help
    
    // read mode
    char *mode = argv[1];

    if (strcmp(mode, "count") != 0 && strcmp(mode, "instance") != 0) {
        fprintf(stderr, "Invalid mode: %s\n", mode);
        return 1;
    }

    // read filepath
    char *filepath = argv[2];

    // read target word
    char *target_word = argv[3];

    printf("Mode: %s\n", mode);
    printf("Filepath: %s\n", filepath);
    printf("Target word: %s\n", target_word);

    if (strcmp(mode, "count") == 0) {
        int count = search_count(filepath, target_word);
        printf("Found: %d of %s in %s\n", count, target_word, filepath);
    } else  if (strcmp(mode, "instance") == 0) {
        struct count_result result = search_instance(filepath, target_word);
        printf("Found: %d of %s in %s\n", result.count, target_word, filepath);
        for (int i = 0; i < result.count; i++) {
            printf("res.instances[%d]: %s\n", i, result.instances[i]);
        }
    }

    return 0;
}