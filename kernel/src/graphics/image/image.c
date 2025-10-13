#include "image.h"
#include "graphics/graphics.h"
#include "graphics/image/bmp.h"
#include "memory/memory.h"
#include "fs/file.h"
#include "lib/vector.h"
#include "status.h"
#include "string/string.h"
#include "memory/heap/kheap.h"

struct vector *image_formats = NULL;

struct image_format *graphics_image_format_get(const char *mime_type)
{
	struct image_format *format = NULL;
	for (size_t i = 0; i < vector_count(image_formats); i++)
	{
		struct image_format *current_format = NULL;
		int res = vector_at(image_formats, i, &current_format, sizeof(struct image_format *));
		if (res < 0)
		{
			break;
		}

		if (current_format && strncmp(current_format->mime_type, mime_type, sizeof(current_format->mime_type)) == 0)
		{
			format = current_format;
			break;
		}
	}

	return format;
}

struct image *graphics_image_load_from_memory(void *memory, size_t max)
{
	struct image *image_out = NULL;
	size_t total_formats = vector_count(image_formats);
	for (size_t i = 0; i < total_formats; i++)
	{
		struct image_format *format = NULL;
		int res = vector_at(image_formats, i, &format, sizeof(format));
		if (res < 0)
		{
			break;
		}

		image_out = format->load(memory, max);
		if (image_out)
		{
			image_out->format = format;
			break;
		}
	}

	return image_out;
}

image_pixel_data graphics_image_get_pixel(struct image *img, int x, int y)
{
	image_pixel_data pixel = img->data[y * img->width + x];
	return pixel;
}

void graphics_image_free(struct image *img)
{
	img->format->free(img);
}

struct image *graphics_image_load(const char *path)
{
	struct image *img = NULL;
	void *img_memory = NULL;
	int fd = 0;
	int res = 0;

	fd = fopen(path, "r");
	if (fd < 0)
	{
		res = fd;
		goto out;
	}

	struct file_stat stat;
	res = fstat(fd, &stat);
	if (res < 0)
	{
		goto out;
	}

	img_memory = kzalloc(stat.filesize);
	if (!img_memory)
	{
		res = -ENOMEM;
		goto out;
	}

	res = fread(img_memory, stat.filesize, 1, fd);
	if (res < 0)
	{
		goto out;
	}

	img = graphics_image_load_from_memory(img_memory, stat.filesize);
	if (!img)
	{
		res = -EINFORMAT;
		goto out;
	}

out:
	fclose(fd);

	if (img_memory)
	{
		kfree(img_memory);
		img_memory = NULL;
	}

	if (res < 0)
	{
		if (img)
		{
			graphics_image_free(img);
			img = NULL;
		}
	}

	return img;
}

int graphics_image_format_register(struct image_format *format)
{
	int res = 0;
	struct image_format *existing_format = graphics_image_format_get(format->mime_type);
	if (existing_format)
	{
		res = -EISTKN;
		goto out;
	}

	res = vector_push(image_formats, &format);
	if (res < 0)
	{
		goto out;
	}

	res = format->register_format(format);
	if (res < 0)
	{
		goto out;
	}

out:
	if (res < 0)
	{
		if (format)
		{
			graphics_image_formats_unload(format);
		}
	}
	return res;
}

int graphics_image_formats_load();

int graphics_image_formats_init()
{
	int res = 0;
	image_formats = vector_new(sizeof(struct image_format *), 10, VECTOR_NO_FLAGS);
	if (!image_formats)
	{
		res = -ENOMEM;
		goto out;
	}

	res = graphics_image_formats_load();
	if (res < 0)
	{
		goto out;
	}

out:
	if (res < 0)
	{
		if (image_formats)
		{
			vector_free(image_formats);
			image_formats = NULL;
		}
	}

	return res;
}

int graphics_image_formats_load()
{
	graphics_image_format_register(graphics_image_format_bmp_setup());
	return 0;
}

void graphics_image_format_unload(struct image_format *format)
{
	if (format->unregister_format)
	{
		format->unregister_format(format);
	}

	if (format->private)
	{
		kfree(format->private);
		format->private = NULL;
	}

	kfree(format);
}

void graphics_image_formats_unload()
{
	while (vector_count(image_formats) > 0)
	{
		struct image_format *format = NULL;
		vector_back(image_formats, &format, sizeof(struct image_format *));
		if (format)
		{
			graphics_image_format_unload(format);
		}

		vector_pop(image_formats);
	}
}
