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
    
    // read mode
    char *mode = argv[1];
    
    // read filepath
    char *filepath = argv[2];

    // read target word
    char *target_word = argv[3];

    // 3. Use the string
    printf("The mode argument you passed is: %s\n", mode);
    printf("The filepath argument you passed is: %s\n", filepath);
    printf("The target word argument you passed is: %s\n", target_word);

    return 0;
}