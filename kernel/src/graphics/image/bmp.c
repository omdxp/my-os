#include "bmp.h"
#include "memory/heap/kheap.h"
#include "string/string.h"
#include "memory/memory.h"
#include "status.h"

struct image *bmp_img_load(void *memory, size_t size)
{
	int res = 0;
	struct image *img = NULL;
	struct bmp_header *header = NULL;
	struct bmp_image_header *bmp_image_header = NULL;

	if (size < sizeof(struct bmp_header))
	{
		res = -EINFORMAT;
		goto out;
	}

	header = (struct bmp_header *)memory;

	// check signature
	if (memcmp(header->bf_type, BMP_SIGNATURE, sizeof(header->bf_type)) != 0)
	{
		res = -EINFORMAT;
		goto out;
	}

	// check if the offset to the pixel data is valid
	if (header->bf_off_bits >= size)
	{
		res = -EINFORMAT;
		goto out;
	}

	bmp_image_header = (struct bmp_image_header *)((uintptr_t)header + sizeof(struct bmp_header));
	img = kzalloc(sizeof(struct image));
	if (!img)
	{
		res = -ENOMEM;
		goto out;
	}

	// check compression
	if (bmp_image_header->bi_compression != BMP_COMPRESSION_UNCOMPRESSED)
	{
		res = -EUNIMP; // only uncompressed is supported
		goto out;
	}

	uint16_t bits_per_pixel = bmp_image_header->bi_bits_per_pixel;
	if (bits_per_pixel != BIT_PER_PIXEL_16777216_COLORS)
	{
		res = -EUNIMP; // only 24-bit BMP is supported
		goto out;
	}

	uint16_t bytes_per_pixel = bits_per_pixel / 8;
	bool bottom_up = (bmp_image_header->bi_height > 0);
	int32_t height = bottom_up ? bmp_image_header->bi_height : -bmp_image_header->bi_height;
	int32_t width = bmp_image_header->bi_width;
	if (width <= 0 || height <= 0)
	{
		res = -EINFORMAT;
		goto out;
	}

	// prepare image
	img->width = width;
	img->height = height;

	size_t pixel_data_size = (size_t)width * (size_t)height * sizeof(image_pixel_data);
	img->data = kzalloc(pixel_data_size);
	if (!img->data)
	{
		res = -ENOMEM;
		goto out;
	}

	// row sizes
	size_t raw_row_size = (size_t)width * (size_t)bytes_per_pixel;
	size_t padded_row_size = (raw_row_size + 3) & ~3; // rows are aligned to 4 bytes

	// make sure to not overflow
	if ((header->bf_off_bits + (padded_row_size * (size_t)height)) > size)
	{
		res = -EINFORMAT;
		goto out;
	}

	uint8_t *bmp_first_pixel_ptr = (uint8_t *)header + header->bf_off_bits;

	// read rows
	for (int row = 0; row < height; row++)
	{
		uint8_t *row_ptr = bmp_first_pixel_ptr + (row * padded_row_size);
		int dest_row = bottom_up ? (height - 1 - row) : row;
		for (int col = 0; col < width; col++)
		{
			uint8_t *bmp_pixel = row_ptr + (col * 3);
			uint8_t b = bmp_pixel[0];
			uint8_t g = bmp_pixel[1];
			uint8_t r = bmp_pixel[2];

			// write image buffer
			size_t pixel_index = (size_t)dest_row * (size_t)width + (size_t)col;
			image_pixel_data *out_pix = &((image_pixel_data *)img->data)[pixel_index];
			out_pix->r = r;
			out_pix->g = g;
			out_pix->b = b;
			out_pix->a = 0; // no alpha in BMP
		}
	}

out:
	if (res < 0)
	{
		if (img)
		{
			if (img->data)
			{
				kfree(img->data);
			}
			kfree(img);
			img = NULL;
		}
	}
	return img;
}

void bmp_img_free(struct image *img)
{
	if (img)
	{
		if (img->data)
		{
			kfree(img->data);
		}
		kfree(img);
	}
}

int bmp_img_format_register(struct image_format *format)
{
	return 0;
}

void bmp_img_format_unregister(struct image_format *format)
{
}

struct image_format bmp_img_format = {
	.mime_type = "image/bmp",
	.load = bmp_img_load,
	.free = bmp_img_free,
	.register_format = bmp_img_format_register,
	.unregister_format = bmp_img_format_unregister,
	.private = NULL};

struct image_format *graphics_image_format_bmp_setup()
{
	return &bmp_img_format;
}
