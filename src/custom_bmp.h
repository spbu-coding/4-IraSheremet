#ifndef BMP_HANDLER_H
#define BMP_HANDLER_H

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BITMAPFILEHEADER_SIZE 14
#define BITMAPINFOHEADER_SIZE 40
#define FULL_BITMAPHEADER_SIZE 54
#define PLANES_NUM 1
#define BYTES_PER_COLOR_IN_TABLE 4
#define BPP_8BIT 8
#define BPP_24BIT 24
#define MAX_COLORS_IN_PALETTE 256

struct color_t {
	unsigned char red, green, blue;
};

struct coord_t {
	int by_width, by_height;
};

struct bmp_header_t {
	uint16_t file_type;
	uint32_t file_size;
	uint16_t reserved1;
	uint16_t reserved2;
	uint32_t off_bits;
	uint32_t header_size;
	int32_t width;
	int32_t height;
	uint16_t planes;
	uint16_t bit_count;
	uint32_t compression;
	uint32_t size_image;
	int32_t x_pels_per_meter;
	int32_t y_pels_per_meter;
	uint32_t clr_used;
	uint32_t clr_important;
};

struct bmp_file_t {
	struct bmp_header_t header;
	struct color_t* palette;
	uint8_t** pixel_array_8bpp;
	struct color_t** pixel_array_24bpp;
};

int check_format(char* name);

int read_bmp_file(char* filename, struct bmp_file_t* bmp);

int write_bmp_file(char* filename, struct bmp_file_t* bmp);

void make_bmp_negative(struct bmp_file_t* bmp);

void free_bmp(struct bmp_file_t* bmp);

int compare_bmp(struct bmp_file_t* picture1, struct bmp_file_t* picture2, struct coord_t* mismatching_pixels);

#endif
