#include "custom_bmp.h"

int check_format(char* name) {
	char* ending = ".bmp";
	unsigned int name_length = strlen(name), ending_length = strlen(ending);
	if (strcmp(name + name_length - ending_length, ending) != 0)
		return -1;
	else
		return 0;
}
uint16_t read_uint16(uint8_t* buffer, unsigned int* buffer_counter) {
	buffer += *buffer_counter;
	uint16_t result = buffer[0] | (buffer[1] << 8u);
	*buffer_counter += sizeof(uint16_t);
	return result;
}

uint32_t read_uint32(uint8_t *buffer, unsigned int *buffer_counter) {
	buffer += *buffer_counter;
	uint32_t result = buffer[0] | buffer[1] << 8u | buffer[2] << 2 * 8u | buffer[3] << 3 * 8u;
	*buffer_counter += sizeof(uint32_t);
	return result;
}

int32_t read_int32(uint8_t* buffer, unsigned int* buffer_counter) {
	buffer += *buffer_counter;
	int32_t size = 8;
	int32_t result = buffer[0] | buffer[1] << 8u | buffer[2] << 2 * 8u | buffer[3] << 3 * 8u;
	*buffer_counter += sizeof(int32_t);
	return result;
}

int read_bmp_header(FILE* input_file, struct bmp_header_t* bmp_header) {
	uint8_t buffer[FULL_BITMAPHEADER_SIZE];
	if (fread(buffer, sizeof(uint8_t), FULL_BITMAPHEADER_SIZE, input_file) != FULL_BITMAPHEADER_SIZE) {
		fprintf(stderr, "Could not read the file header\n");
		return -1;
	}
	unsigned int buffer_counter = 0;
	bmp_header->file_type = read_uint16(buffer, &buffer_counter);
	if (bmp_header->file_type != 0x4D42) {
		fprintf(stderr, "The file format is not bmp: expected BM as first two bytes, got %c\n", bmp_header->file_type);
		return -1;
	}
	bmp_header->file_size = read_uint32(buffer, &buffer_counter);
	bmp_header->reserved1 = read_uint16(buffer, &buffer_counter);
	bmp_header->reserved2 = read_uint16(buffer, &buffer_counter);
	if (bmp_header->reserved1 != 0 || bmp_header->reserved2 != 0) {
		fprintf(stderr, "Reserved bytes must be = 0\n");
		return - 1;
	}
	bmp_header->off_bits = read_uint32(buffer, &buffer_counter);
	bmp_header->header_size = read_uint32(buffer, &buffer_counter);
    if (bmp_header->header_size != BITMAPINFOHEADER_SIZE) {
	    fprintf(stderr, "This is not a BMP version 3 file: expected 40 as info header size\n");
		return -1;
	}
	bmp_header->width = read_int32(buffer, &buffer_counter);
	if (bmp_header->width <= 0) {
		fprintf(stderr, "The width must be positive\n");
		return -1;
	}
	bmp_header->height = read_int32(buffer, &buffer_counter);
	if (bmp_header->height == 0) {
		fprintf(stderr, "The height must be non zero\n");
		return -1;
	}
	bmp_header->planes = read_uint16(buffer, &buffer_counter);
	if (bmp_header->planes != 1) {
		fprintf(stderr, "Planes must be = 1\n");
		return -1;
	}
	bmp_header->bit_count = read_uint16(buffer, &buffer_counter);
	if (bmp_header->bit_count != BPP_8BIT && bmp_header->bit_count != BPP_24BIT) {
		fprintf(stderr, "Supported only 8 and 24 bits for pixel\n");
		return -1;
	}
	bmp_header->compression = read_uint32(buffer, &buffer_counter);
	if (bmp_header->compression != 0) {
		fprintf(stderr, "Supported only uncompressed images\n");
		return -1;
	}
	bmp_header->size_image = read_uint32(buffer, &buffer_counter);
	bmp_header->x_pels_per_meter = read_int32(buffer, &buffer_counter);
	bmp_header->y_pels_per_meter = read_int32(buffer, &buffer_counter);
	bmp_header->clr_used = read_uint32(buffer, &buffer_counter);
	bmp_header->clr_important = read_uint32(buffer, &buffer_counter);
	if ((bmp_header->clr_used != 0 && bmp_header->clr_important > bmp_header->clr_used) || bmp_header->clr_used > MAX_COLORS_IN_PALETTE) {
		fprintf(stderr, "Important size of color table is greater than size of color table\n");
		return -1;
	}
	return 0;
}

int read_bmp_palette(FILE* input_file, struct color_t** palette, uint32_t* clr_used) {
	uint32_t clr_used_num = *clr_used;
	if (*clr_used == 0) {
		clr_used_num = MAX_COLORS_IN_PALETTE;
	}
	*palette = (struct color_t*)(malloc(clr_used_num * sizeof(struct color_t)));
	if (*palette == NULL) {
		fprintf(stderr, "Could not allocate memory for palette\n");
		return -2;
	}
	unsigned int palette_size = clr_used_num * BYTES_PER_COLOR_IN_TABLE;
	unsigned char buffer[palette_size];
	unsigned int buffer_counter = 0;
	if (fread(buffer, sizeof(unsigned char), palette_size, input_file) != (palette_size)) {
		fprintf(stderr, "Could not read the palette\n");
		free(*palette);
		return -2;
	}
	for (uint32_t i = 0; i < clr_used_num; i++) {
		unsigned char color[BYTES_PER_COLOR_IN_TABLE];
		for (unsigned j = 0; j < BYTES_PER_COLOR_IN_TABLE; j++) {
			color[j] = buffer[buffer_counter];
			buffer_counter++;
		}
		*palette[i] = (struct color_t){ .blue = color[0], .green = color[1], .red = color[2] };
		if (color[3] != 0) {
			fprintf(stderr, "Fourth (reserved) byte of color #%d in color table is not equal to zero\n", i + 1);
			return -1;
		}
	}
	free(buffer);
	return 0;
}

int read_bmp_pixel_array_8bpp(FILE* input_file, struct bmp_file_t* bmp) {
	uint32_t height_module;
	if (bmp->header.height < 0)
		height_module = -bmp->header.height;
	else
		height_module = bmp->header.height;
	int32_t amount_of_extra_bytes = (4 - bmp->header.width % 4) % 4, width_of_row_in_bytes = bmp->header.width + amount_of_extra_bytes;
	bmp->pixel_array_8bpp = (uint8_t**)(malloc(height_module * sizeof(uint8_t*)));
	if (bmp->pixel_array_8bpp == NULL) {
		fprintf(stderr, "Could not allocate memory for pixel array\n");
		return -2;
	}

	unsigned int buffer_size = height_module * width_of_row_in_bytes * sizeof(uint8_t);
	uint8_t* buffer = (uint8_t*)(malloc(buffer_size));
	unsigned int buffer_counter = 0;
	if (buffer == NULL) {
		fprintf(stderr, "Could not allocate memory for pixel array\n");
		return -2;
	}
	if (fread(buffer, sizeof(uint8_t), buffer_size, input_file) != (int)(buffer_size)) {
		if (feof(input_file) > 0) {
			fprintf(stderr, "Height and/or width do not match real file size\n");
			return -1;
		}
		fprintf(stderr, "Could not read pixel array\n");
		return -2;
	}
	for (uint32_t i = 0; i < height_module; i++) {
		bmp->pixel_array_8bpp[i] = (uint8_t*)(malloc(bmp->header.width * sizeof(uint8_t)));
		if (bmp->pixel_array_8bpp[i] == NULL) {
			fprintf(stderr, "Could not allocate memory for row %d of pixel array\n", i);
			return -1;
		}
		for (uint32_t j = 0; j < bmp->header.width; j++) {
			bmp->pixel_array_8bpp[i][j] = buffer[buffer_counter];
			buffer_counter++;
		}
		if (bmp->header.width % 4 != 0) {
			uint8_t extra_bytes[3];
			for (uint32_t j = 0; j < amount_of_extra_bytes; j++) {
				extra_bytes[j] = buffer[buffer_counter];
				buffer_counter++;
			}
			if ((extra_bytes[0] != 0) || (amount_of_extra_bytes > 1 && extra_bytes[1] != 0) || (amount_of_extra_bytes > 2 && extra_bytes[2] != 0)) {
				fprintf(stderr, "Some of extra bytes in row are not equal to zero\n");
				return -1;
			}
		}
	}
	return 0;
}

int read_bmp_pixel_array_24bpp(FILE* input_file, struct bmp_file_t* bmp) {
	uint32_t height_module;
	if (bmp->header.height < 0)
		height_module = -bmp->header.height;
	else
		height_module = bmp->header.height;
	int32_t width_of_row_in_bytes = bmp->header.width * 3;
	width_of_row_in_bytes += (4 - width_of_row_in_bytes % 4) % 4;
	bmp->pixel_array_24bpp = (struct color_t**)(malloc(height_module * sizeof(struct color_t*)));
	if (bmp->pixel_array_24bpp == NULL) {
		fprintf(stderr, "Could not allocate memory for pixel array\n");
		return -2;
	}
	unsigned int buffer_size = height_module * width_of_row_in_bytes;
	uint8_t* buffer = (uint8_t*)(malloc(buffer_size));
	unsigned int buffer_counter = 0;
	if (buffer == NULL) {
		fprintf(stderr, "Could not allocate memory for pixel array\n");
		return -2;
	}
	if (fread(buffer, sizeof(uint8_t), buffer_size, input_file) != (int)(buffer_size)) {
		if (feof(input_file) > 0) {
			fprintf(stderr, "Height and/or width do not match real file size\n");
			return -1;
		}
		fprintf(stderr, "Could not read pixel array\n");
		return -2;
	}
	for (uint32_t i = 0; i < height_module; i++) {
		bmp->pixel_array_24bpp[i] = (struct color_t*)(malloc(bmp->header.width * sizeof(struct color_t)));
		if (bmp->pixel_array_24bpp[i] == NULL) {
			fprintf(stderr, "Could not allocate memory for row of pixel array\n");
			return -2;
		}
		for (uint32_t j = 0; j < bmp->header.width; j++) {
			uint8_t color[3];
			for (uint32_t k = 0; k < 3; k++) {
				color[k] = buffer[buffer_counter];
				buffer_counter++;
			}
			bmp->pixel_array_24bpp[i][j] = (struct color_t){ .blue = color[0], .green = color[1], .red = color[2] };
		}
		if (bmp->header.width % 4 != 0) { 
			uint32_t amount_of_extra_bytes = bmp->header.width % 4;
			uint8_t extra_bytes[3];
			for (uint32_t j = 0; j < amount_of_extra_bytes; j++) {
				extra_bytes[j] = buffer[buffer_counter];
				buffer_counter++;
			}
			if ((extra_bytes[0] != 0) || (amount_of_extra_bytes > 1 && extra_bytes[1] != 0) || (amount_of_extra_bytes == 3 && extra_bytes[2] != 0)) {
				fprintf(stderr, "Some of extra bytes in row are not equal to zero\n");
				return -1;
			}
		}
	}
	return 0;
}
int read_bmp_file(char* filename, struct bmp_file_t* bmp) {
	FILE* input_file = fopen(filename, "rb");
	if (input_file == NULL) {
		fprintf(stderr, "Could not open file %s\n", filename);
		return -2;
	}
	if (read_bmp_header(input_file, &bmp->header) != 0) {
		fclose(input_file);
		return -1;
	}
	if (bmp->header.bit_count == BPP_8BIT) {
		if (read_bmp_palette(input_file, &bmp->palette, &bmp->header.clr_used) != 0) {
			fclose(input_file);
			return -2;
		}
		int result = read_bmp_pixel_array_8bpp(input_file, bmp);
		if (result != 0) {
			fclose(input_file);
			return result;
		}
	}
	else {
		int result = read_bmp_pixel_array_24bpp(input_file, bmp);
		if (result != 0) {
			fclose(input_file);
			return result;
		}
	}
	if (fclose(input_file) != 0) {
		fprintf(stderr, "Could not close bmp file\n");
		return -1;
	}
	return 0;
}

struct color_t convert_to_negative(struct color_t color) {
	struct color_t result = { .red = ~(color.red), .green = ~(color.green), .blue = ~(color.blue) };
	return result;
}

void make_bmp_negative(struct bmp_file_t* bmp) {
	if (bmp->header.bit_count == BPP_8BIT) {
		uint32_t clr_used = bmp->header.clr_used;
		if (clr_used == 0)
			clr_used = MAX_COLORS_IN_PALETTE;
		for (uint32_t i = 0; i < clr_used; i++) {
			bmp->palette[i] = convert_to_negative(bmp->palette[i]);
		}
	}
	else {
		uint32_t height_module;
		if (bmp->header.height < 0)
			height_module = -bmp->header.height;
		else
			height_module = bmp->header.height;
		for (uint32_t i = 0; i < height_module; i++) {
			for (int j = 0; j < bmp->header.width; j++) {
				bmp->pixel_array_24bpp[i][j] = convert_to_negative(bmp->pixel_array_24bpp[i][j]);
			}
		}
	}
}

void write_uint16(uint16_t* word, uint8_t* buffer, unsigned int* buffer_counter) {
	buffer += *buffer_counter;
	uint16_t max = 255, size = 8;
	buffer[0] = *word & max;
	buffer[1] = *word >> size;
	*buffer_counter += sizeof(uint16_t);
}

void write_uint32(uint32_t* word, uint8_t* buffer, unsigned int* buffer_counter) {
	buffer += *buffer_counter;
	const uint32_t size = 8, max = 255;
	uint32_t num = *word;
	for (int i = 0; i < 4; i++) {
		buffer[i] = num & max;
		num >>= size;
	}
	*buffer_counter += sizeof(uint32_t);
}

void write_int32(int32_t* word, uint8_t* buffer, unsigned int* buffer_counter) {
	buffer += *buffer_counter;
	const int32_t size = 8, max = 255;
	int32_t num = *word;
	for (int i = 0; i < 4; i++) {
		buffer[i] = num & max;
		num>>= size;
	}
	*buffer_counter += sizeof(int32_t);
}

int write_bmp_header(struct bmp_header_t* bmp_header, FILE* output_file) {
	uint8_t buffer[FULL_BITMAPHEADER_SIZE];
	unsigned int buffer_counter = 0;
	write_uint16(&bmp_header->file_type, buffer, &buffer_counter);
	write_uint32(&bmp_header->file_size, buffer, &buffer_counter);
	write_uint16(&bmp_header->reserved1, buffer, &buffer_counter);
	write_uint16(&bmp_header->reserved2, buffer, &buffer_counter);
	write_uint32(&bmp_header->off_bits, buffer, &buffer_counter);
	write_uint32(&bmp_header->header_size, buffer, &buffer_counter);
	write_int32(&bmp_header->width, buffer, &buffer_counter);
	write_int32(&bmp_header->height, buffer, &buffer_counter);
	write_uint16(&bmp_header->planes, buffer, &buffer_counter);
	write_uint16(&bmp_header->bit_count, buffer, &buffer_counter);
	write_uint32(&bmp_header->compression, buffer, &buffer_counter);
	write_uint32(&bmp_header->size_image, buffer, &buffer_counter);
	write_int32(&bmp_header->x_pels_per_meter, buffer, &buffer_counter);
	write_int32(&bmp_header->y_pels_per_meter, buffer, &buffer_counter);
	write_uint32(&bmp_header->clr_used, buffer, &buffer_counter);
	write_uint32(&bmp_header->clr_important, buffer, &buffer_counter);
	if (fwrite(buffer, sizeof(uint8_t), FULL_BITMAPHEADER_SIZE, output_file) != FULL_BITMAPHEADER_SIZE) {
		fprintf(stderr, "Could not write bmp header to output file\n");
		return -1;
	}
	return 0;
}

void write_in_buffer(uint8_t* buffer, unsigned int* buffer_counter, unsigned char* source, unsigned int amount_of_bytes) {
	buffer += *buffer_counter;
	for (unsigned int i = 0; i < amount_of_bytes; i++) {
		buffer[i] = source[i];
	}
}

int write_bmp_pixel_array_24bpp(FILE* output_file, struct bmp_file_t* bmp) {
	uint32_t height_module;
	if (bmp->header.height < 0)
		height_module = -bmp->header.height;
	else
		height_module = bmp->header.height;
	int32_t width_of_row_in_bytes = bmp->header.width * 3;
	uint32_t amount_of_extra_bytes = bmp->header.width % 4;
	if (amount_of_extra_bytes != 0) {
		width_of_row_in_bytes += amount_of_extra_bytes;
	}
	unsigned int buffer_size = height_module * width_of_row_in_bytes * sizeof(uint8_t);
	uint8_t* buffer = (uint8_t*)(malloc(buffer_size));
	if (buffer == NULL) {
		fprintf(stderr, "Could not allocate memory for buffer with pixel array\n");
		return -2;
	}
	unsigned int buffer_counter = 0;
	for (uint32_t i = 0; i < height_module; i++) {
		for (uint32_t j = 0; j < bmp->header.width; j++) {
			unsigned char color[] = { bmp->pixel_array_24bpp[i][j].blue, bmp->pixel_array_24bpp[i][j].green, bmp->pixel_array_24bpp[i][j].red };
			write_in_buffer(buffer, &buffer_counter, color, 3);
			buffer_counter += 3;
		}
		for (uint32_t j = 0; j < amount_of_extra_bytes; j++) {
			unsigned char extra = 0;
			write_in_buffer(buffer, &buffer_counter, &extra, 1);
			buffer_counter += 1;
		}
	}
	int result = fwrite(buffer, sizeof(uint8_t), buffer_size, output_file);
	if (result != (int)buffer_size) {
		fprintf(stderr, "Could not write pixel array to output file\n");
		free(buffer);
		return -2;
	}
	free(buffer);
	return 0;
}

int write_bmp_palette(FILE* output_file, struct bmp_file_t* bmp) {
	if (bmp->header.clr_used == 0) {
		bmp->header.clr_used = MAX_COLORS_IN_PALETTE;
	}
	unsigned int buffer_size = bmp->header.clr_used * sizeof(uint8_t) * BYTES_PER_COLOR_IN_TABLE;
	uint8_t* buffer = (uint8_t*)(malloc(buffer_size));
	if (buffer == NULL) {
		fprintf(stderr, "Could not allocate memory for buffer with color palette\n");
		return -2;
	}
	unsigned int buffer_counter = 0;
	for (uint32_t i = 0; i < bmp->header.clr_used; i++) {
		unsigned char color[] = { bmp->palette[i].blue, bmp->palette[i].green, bmp->palette[i].red, 0 };
		write_in_buffer(buffer, &buffer_counter, color, BYTES_PER_COLOR_IN_TABLE);
		buffer_counter += BYTES_PER_COLOR_IN_TABLE;
	}
	int result = fwrite(buffer, sizeof(uint8_t), buffer_size, output_file);
	if (result != (int)buffer_size) {
		fprintf(stderr, "Could not write color palette to file\n");
		free(buffer);
		return -2;
	}
	free(buffer);
	return 0;
}

int write_bmp_pixel_array_8bpp(FILE* output_file, struct bmp_file_t* bmp) {
	uint32_t height_module;
	if (bmp->header.height < 0)
		height_module = -bmp->header.height;
	else
		height_module = bmp->header.height;
	int32_t amount_of_extra_bytes = (4 - bmp->header.width % 4) % 4, width_of_row_in_bytes = bmp->header.width + amount_of_extra_bytes;
	unsigned int buffer_size = height_module * width_of_row_in_bytes * sizeof(uint8_t);
	uint8_t* buffer = (uint8_t*)(malloc(buffer_size));
	if (buffer == NULL) {
		fprintf(stderr, "Could not allocate memory for buffer with pixel array\n");
		return -2;
	}
	unsigned int buffer_counter = 0;
	for (uint32_t i = 0; i < height_module; i++) {
		for (uint32_t j = 0; j < bmp->header.width; j++) {
			unsigned char color_number = bmp->pixel_array_8bpp[i][j];
			write_in_buffer(buffer, &buffer_counter, &color_number, BPP_8BIT);
		}
		for (uint32_t j = 0; j < amount_of_extra_bytes; j++) {
			unsigned char extra = 0;
			write_in_buffer(buffer, &buffer_counter, &extra, 1);
		}
	}
	int result = fwrite(buffer, sizeof(uint8_t), buffer_size, output_file);
	if (result != (int)buffer_size) {
		fprintf(stderr, "Could not write pixel array to file\n");
		free(buffer);
		return -2;
	}
	free(buffer);
	return 0;
}

int write_bmp_file(char* filename, struct bmp_file_t* bmp) {
	FILE* output_file = fopen(filename, "wb");
	if (output_file == NULL) {
		fprintf(stderr, "Could not create bmp file\n");
		return -1;
	}
	if (write_bmp_header(&bmp->header, output_file) != 0) {
		fclose(output_file);
		return -1;
	}
	if (bmp->header.bit_count == BPP_24BIT) {
		if (write_bmp_pixel_array_24bpp(output_file, bmp) != 0) {
			fclose(output_file);
			return -2;
		}
	}
	else {
		if (write_bmp_palette(output_file, bmp) != 0) {
			fclose(output_file);
			return -2;
		}
		if (write_bmp_pixel_array_8bpp(output_file, bmp) != 0) {
			fclose(output_file);
			return -2;
		}
	}
	fclose(output_file);
	return 0;
}

void free_bmp(struct bmp_file_t* bmp) {
	if (bmp->header.bit_count == BPP_24BIT) {
		uint32_t height_module;
		if (bmp->header.height < 0)
			height_module = -bmp->header.height;
		else
			height_module = bmp->header.height;
		for (uint32_t i = 0; i < height_module; i++) {
			free(bmp->pixel_array_24bpp[i]);
		}
		free(bmp->pixel_array_24bpp);
	}
	else {
		free(bmp->palette);
		uint32_t height_module;
		if (bmp->header.height < 0)
			height_module = -bmp->header.height;
		else
			height_module = bmp->header.height;
		for (uint32_t i = 0; i < height_module; i++) {
			free(bmp->pixel_array_8bpp[i]);
		}
		free(bmp->pixel_array_8bpp);
	}
}

int check_if_colors_are_equal(struct color_t color1, struct color_t color2) {
	if (color1.red == color2.red && color1.green == color2.green && color1.blue == color2.blue)
		return 0;
	else
		return -1;
}

struct color_t get_pixel_color(struct bmp_file_t* picture, uint32_t coord_by_width, uint32_t coord_by_height) {
	if (picture->header.height < 0) {
		coord_by_height = -picture->header.height - coord_by_height - 1;
	}
	if (picture->header.bit_count == BPP_24BIT) {
		return picture->pixel_array_24bpp[coord_by_height][coord_by_width];
	}
	else {
		return picture->palette[picture->pixel_array_8bpp[coord_by_height][coord_by_width]];
	}
}

int compare_bmp(struct bmp_file_t* picture1, struct bmp_file_t* picture2, struct coord_t* mismatching_pixels) {
	if (picture1->header.width != picture2->header.width) {
		fprintf(stderr, "The width of the first picture != the width of the second picture\n");
		return -1;
	}

	if (picture1->header.height != picture2->header.height && picture1->header.height != -picture2->header.height) {
		fprintf(stderr, "The height of the first picture is not equal to the height of the second picture\n");
		return -1;
	}
	int number_of_mismatching_pixels = 0;
	uint32_t height_module;
	if (picture1->header.height < 0)
		height_module = -picture1->header.height;
	else
		height_module = picture1->header.height;
	for (uint32_t i = 0; i < height_module; i++) {
		for (uint32_t j = 0; j < picture1->header.width; j++) {
			if (check_if_colors_are_equal(get_pixel_color(picture1, j, i), get_pixel_color(picture2, j, i)) != 0) {
				if (number_of_mismatching_pixels < 100) {
					mismatching_pixels[number_of_mismatching_pixels] = (struct coord_t){ .by_height = i, .by_width = j };
					number_of_mismatching_pixels++;
				}
			}
		}
	}
	return number_of_mismatching_pixels;
}