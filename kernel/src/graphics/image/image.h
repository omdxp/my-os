#pragma once

#include <stdint.h>
#include <stddef.h>

struct image_format;
typedef union image_pixel_data
{
	uint32_t data; // 32-bit representation of pixel data
	struct
	{
		uint8_t r; // Red channel
		uint8_t g; // Green channel
		uint8_t b; // Blue channel
		uint8_t a; // Alpha channel
	};
} image_pixel_data;

struct image
{
	uint32_t width;				 // Width of the image in pixels
	uint32_t height;			 // Height of the image in pixels
	image_pixel_data *data;		 // Pointer to the pixel data
	void *private;				 // Pointer to private data (e.g., for specific image formats)
	struct image_format *format; // Pointer to the image format
};

typedef struct image *(*image_load_function)(void *memory, size_t size);
typedef void (*image_free_function)(struct image *img);
typedef int (*image_format_register_function)(struct image_format *format);
typedef void (*image_format_unregister_function)(struct image_format *format);

#define IMAGE_FORMAT_MAX_MIME_TYPE 64
struct image_format
{
	char mime_type[IMAGE_FORMAT_MAX_MIME_TYPE]; // MIME type of the image format (e.g., "image/png")
	image_load_function load;					// Function to load an image of this format
	image_free_function free;					// Function to free an image of this format

	image_format_register_function register_format;		// Function to register this image format
	image_format_unregister_function unregister_format; // Function to unregister this image format

	void *private; // Pointer to private data (e.g., for specific image format handlers)
};

int graphics_image_formats_init();
void graphics_image_formats_unload();
struct image *graphics_image_load(const char *path);
image_pixel_data graphics_image_get_pixel(struct image *img, uint32_t x, uint32_t y);
struct image *graphics_image_load_from_memory(void *memory, size_t max);
struct image_format *graphics_image_format_get(const char *mime_type);
