#pragma once

#include "image.h"
#include <stdint.h>
#include <stddef.h>

#define BIT_PER_PIXEL_MONOCHROME 1
#define BIT_PER_PIXEL_16_COLORS 4
#define BIT_PER_PIXEL_256_COLORS 8
#define BIT_PER_PIXEL_65536_COLORS 16
#define BIT_PER_PIXEL_16777216_COLORS 24

#define BMP_SIGNATURE "BM"

#define BMP_COMPRESSION_UNCOMPRESSED 0
#define BMP_COMPRESSION_RLE8 1
#define BMP_COMPRESSION_RLE4 2
#define BMP_COMPRESSION_BITFIELDS 3

struct bmp_header
{
	char bf_type[2];	   // Signature "BM"
	uint32_t bf_size;	   // Size of the BMP file in bytes
	uint16_t bf_reserved1; // Reserved; must be zero
	uint16_t bf_reserved2; // Reserved; must be zero
	uint32_t bf_off_bits;  // Offset to the start of the pixel data
} __attribute__((packed));

struct bmp_image_header
{
	uint32_t bi_size;			 // Size of this header (40 bytes)
	int32_t bi_width;			 // Width of the bitmap in pixels
	int32_t bi_height;			 // Height of the bitmap in pixels
	uint16_t bi_planes;			 // Number of color planes (must be 1)
	uint16_t bi_bits_per_pixel;	 // Number of bits per pixel
	uint32_t bi_compression;	 // Compression method being used
	uint32_t bi_size_image;		 // Size of the raw bitmap data (including padding)
	int32_t bi_x_pels_per_meter; // Horizontal resolution (pixels per meter)
	int32_t bi_y_pels_per_meter; // Vertical resolution (pixels per meter)
	uint32_t bi_clr_used;		 // Number of colors in the color palette
	uint32_t bi_clr_important;	 // Number of important colors used
} __attribute__((packed));

struct color_table
{
	uint8_t red;	  // Red component
	uint8_t green;	  // Green component
	uint8_t blue;	  // Blue component
	uint8_t reserved; // Reserved (usually set to 0)
};

struct image_format *graphics_image_format_bmp_setup();
