#include <stdio.h>
#include <string.h>
#include "custom_bmp.h"

#define NUMBER_OF_PARAMETERS 3

int parse_arguments(int argc, char* argv[]) {
	if (argc != NUMBER_OF_PARAMETERS) {
		fprintf(stderr, "Incorrect number of parameters: expected 3, received %d\n", argc);
		return -2;
	}
	for (int i = 1; i <= 2; i++) {
		if (check_format(argv[i]) != 0) {
			fprintf(stderr, "The file %s is not bmp\n", argv[i]);
			return -2;
		}
	}
	return 0;
}

int main(int argc, char* argv[]) {
	char* files_names[2];
	if (parse_arguments(argc, argv) != 0) 
		return -2;
	struct bmp_file_t pictures[2];
	for (int i = 1; i <= 2; i++) {
		int result = read_bmp_file(argv[i], &pictures[i-1]);
		if (result != 0)
			return result;
	}
	struct coord_t mismatching_pixels[100];
	int number_of_mismatching_pixels = compare_bmp(&pictures[0], &pictures[1], mismatching_pixels);
	for (int i = 0; i < 2; i++) {
		free_bmp(&pictures[i]);
	}
	if (number_of_mismatching_pixels < 0) {
		return -1;
	}
	if (number_of_mismatching_pixels == 0) {
		return 0;
	}
	char* buffer = malloc(1024 * sizeof(char)), *pointer_to_buffer_pos = buffer;
	for (int i = 0; i < number_of_mismatching_pixels; i++) 
		pointer_to_buffer_pos += sprintf(pointer_to_buffer_pos, "%d %d\n", mismatching_pixels[i].by_width, mismatching_pixels[i].by_height);
	fprintf(stderr, "%s", buffer);
	free(buffer);
	return number_of_mismatching_pixels;
}