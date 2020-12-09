#include <stdio.h>
#include <string.h>
#include "custom_bmp.h"
#include "qdbmp_negative.h"

#define NUMBER_OF_PARAMETERS 4

int parse_parameters(int argc, char *argv[], int *my_realisation) {
    if (argc != NUMBER_OF_PARAMETERS) { 
        fprintf(stderr, "Incorrect number of parameters: expected 3, received %d\n", argc);
        return -1;
    }
    if (strcmp(argv[1], "--mine") == 0) {
        *my_realisation = 1;
    }
    else if (strcmp(argv[1], "--theirs") != 0) {
        fprintf(stderr, "Incorrect first parameter\n");
        return -1;
    }
    for (int i = 2; i <= 3; i++) {
        if (check_format(argv[i]) != 0) {
            fprintf(stderr, "The file %s is not bmp\n", argv[i]);
            return -1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    int my_realisation = 0; 
    if (parse_parameters(argc, argv, &my_realisation) != 0)
        return -1;
    FILE* input_file = fopen(argv[2], "rb");
    if (input_file == NULL) {
        fprintf(stderr, "Couldn't open the file %s\n", argv[2]);
        return -2;
    }
    if (my_realisation != 1) {
        char* arg[] = { "qdbmp_negative", argv[2], argv[3] };
        if (qdbmp_negative(3, arg) != 0) {
            fprintf(stderr, "Error in convertesion using a third-party qdbmp library\n");
            return -3;
        }
        return 0;
    }
    struct bmp_file_t input_bmp;
    int result = read_bmp_file(argv[2], &input_bmp);
    if (result != 0) {
        return result;
    }
    make_bmp_negative(&input_bmp);
    result = write_bmp_file(argv[3], &input_bmp);
    if (result != 0) {
        free_bmp(&input_bmp);
        return result;
    }
    free_bmp(&input_bmp);
    return 0;
}

