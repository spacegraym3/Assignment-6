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
    
    char *mode = argv[1];
    char *filepath = argv[2];
    char *target_word = argv[3];

    if (strcmp(mode, "count") == 0) {
        search_count(filepath, target_word);
    } else  if (strcmp(mode, "instance") == 0) {
        search_instance(filepath, target_word);
    }
    return 0;
}