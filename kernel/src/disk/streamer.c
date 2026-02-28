#include "streamer.h"
#include "memory/heap/kheap.h"
#include "memory/memory.h"
#include "config.h"
#include "kernel.h"
#include "status.h"
#include <stdbool.h>

struct disk_stream_cache *diskstreamer_cache_new()
{
	struct disk_stream_cache *cache = kzalloc(sizeof(struct disk_stream_cache));
	if (!cache)
	{
		goto out;
	}

out:
	return cache;
}

struct disk_stream_cache_bucket_level1 *diskstreamer_cache_bucket_level1_get(struct disk_stream_cache *cache, int index)
{
	struct disk_stream_cache_bucket_level1 *level1 = NULL;
	if (index >= cache->total_buckets)
	{
		size_t old_total = cache->total_buckets;
		size_t new_total = index + 1;
		size_t new_size = sizeof(struct disk_stream_cache_bucket_level1 **) * new_total;

		struct disk_stream_cache_bucket_level1 **new_buckets = krealloc(cache->buckets, new_size);
		if (!new_buckets)
		{
			return NULL;
		}

		memset(new_buckets + old_total, 0, sizeof(struct disk_stream_cache_bucket_level1 *) * (new_total - old_total));
		cache->buckets = new_buckets;
		cache->total_buckets = new_total;
	}

	level1 = cache->buckets[index];
	if (!level1)
	{
		cache->buckets[index] = kzalloc(sizeof(struct disk_stream_cache_bucket_level1));
		if (!cache->buckets[index])
		{
			return NULL;
		}

		level1 = cache->buckets[index];
		level1->total_buckets = 0;
	}

	return level1;
}

struct disk_stream_cache_bucket_level2 *diskstreamer_cache_bucket_level2_get(struct disk_stream_cache_bucket_level1 *level1_bucket, int index)
{
	int max_buckets = (sizeof(level1_bucket->buckets) / sizeof(*level1_bucket->buckets));
	if (index >= max_buckets)
	{
		return NULL;
	}

	struct disk_stream_cache_bucket_level2 *level2 = level1_bucket->buckets[index];
	if (!level2)
	{
		level2 = kzalloc(sizeof(struct disk_stream_cache_bucket_level2));
		if (!level2)
		{
			return NULL;
		}

		level2->total_buckets = 0;
		level1_bucket->buckets[index] = level2;
	}

	return level2;
}

struct disk_stream_cache_bucket_level3 *diskstreamer_cache_bucket_level3_get(struct disk_stream_cache_bucket_level2 *level2_bucket, int index)
{
	int max_buckets = (sizeof(level2_bucket->buckets) / sizeof(*level2_bucket->buckets));
	if (index >= max_buckets)
	{
		return NULL;
	}

	struct disk_stream_cache_bucket_level3 *level3 = level2_bucket->buckets[index];
	if (!level3)
	{
		level3 = kzalloc(sizeof(struct disk_stream_cache_bucket_level3));
		if (!level3)
		{
			return NULL;
		}

		level3->total_sectors = 0;
		level2_bucket->buckets[index] = level3;
	}

	return level3;
}

void diskstreamer_cache_bucket_level3_free(struct disk_stream_cache_bucket_level3 *level3_bucket)
{
	for (size_t i = 0; i < DISK_STREAM_LEVEL3_SECTORS_ARRAY_SIZE; i++)
	{
		kfree(level3_bucket->sectors[i]);
	}

	kfree(level3_bucket);
}

int diskstreamer_cache_roundrobin_add(struct disk_stream_cache *cache, struct disk_stream_cache_bucket_level3 *level3_bucket)
{
	int res = 0;
	int index = cache->mem_roundrobin.pos % DISK_STREAM_CACHE_ROUNDROBIN_MAX;
	struct disk_stream_cache_bucket_level3 *old_elem = cache->mem_roundrobin.queue[index];
	if (old_elem)
	{
		old_elem->roundrobin_count--;
		if (old_elem->roundrobin_count <= 0)
		{
			diskstreamer_cache_bucket_level3_free(old_elem);
			cache->mem_roundrobin.queue[index] = NULL;
		}
	}

	cache->mem_roundrobin.queue[index] = level3_bucket;
	cache->mem_roundrobin.queue[index]->roundrobin_count++;
	cache->mem_roundrobin.pos++;

	return res;
}

int diskstreamer_cache_find(struct disk *disk, int pos, struct disk_stream_cache_sector **out_sector)
{
	int res = DISK_STREAMER_CACHE_STATUS_FOUND;
	struct disk_stream_cache *cache = disk->cache;
	if (!cache)
	{
		return -ENOENT;
	}

	long level1_size = DISK_STREAM_BUCKET1_BYTE_SIZE(disk->sector_size);
	long level2_size = DISK_STREAM_BUCKET2_BYTE_SIZE(disk->sector_size);
	long level3_size = DISK_STREAM_BUCKET3_BYTE_SIZE(disk->sector_size);

	long level1_bucket = pos / level1_size;
	long pos_in_level1 = pos % level1_size;
	long level2_bucket = pos_in_level1 / level2_size;
	long pos_in_level2 = pos_in_level1 % level2_size;
	long level3_bucket = pos_in_level2 / level3_size;

	struct disk_stream_cache_bucket_level1 *level1 = diskstreamer_cache_bucket_level1_get(cache, level1_bucket);
	if (!level1)
	{
		return -EINVAL;
	}

	struct disk_stream_cache_bucket_level2 *level2 = diskstreamer_cache_bucket_level2_get(level1, level2_bucket);
	if (!level2)
	{
		return -EINVAL;
	}

	struct disk_stream_cache_bucket_level3 *level3 = diskstreamer_cache_bucket_level3_get(level2, level3_bucket);
	if (!level3)
	{
		return -EINVAL;
	}

	long pos_in_level3 = pos_in_level2 % level3_size;
	int byte_offset = pos_in_level3 % (sizeof(level3->sectors) * disk->sector_size);
	int sector_index = byte_offset / disk->sector_size;
	if (!level3->sectors[sector_index])
	{
		level3->sectors[sector_index] = kzalloc(sizeof(struct disk_stream_cache_sector));
		if (!level3->sectors[sector_index])
		{
			return -ENOMEM;
		}

		level3->total_sectors++;
		diskstreamer_cache_roundrobin_add(cache, level3);
		res = DISK_STREAMER_CACHE_STATUS_NEW_CACHE_ENTRY;
	}

	if (out_sector)
	{
		*out_sector = level3->sectors[sector_index];
	}

	return res;
}

struct disk_stream *diskstreamer_new(int disk_id)
{
	struct disk *disk = disk_get(disk_id);
	if (!disk)
	{
		return 0;
	}

	return diskstreamer_new_from_disk(disk);
}

struct disk_stream *diskstreamer_new_from_disk(struct disk *disk)
{
	struct disk_stream *streamer = kzalloc(sizeof(struct disk_stream));
	streamer->pos = 0;
	streamer->sector_size = disk->sector_size;
	streamer->disk = disk;
	return streamer;
}

int diskstreamer_seek(struct disk_stream *stream, int pos)
{
	stream->pos = pos;
	return 0;
}

int diskstreamer_read(struct disk_stream *stream, void *out, int total)
{
	int res = 0;
	int offset = stream->pos;
	int offset_aligned_down = offset;
	int starting_sector = 0;
	int ending_sector = 0;
	int total_bytes_aligned = 0;
	int total_sectors_to_read = 0;
	int final_offset = 0;
	int final_offset_aligned_up = 0;

	if ((offset % stream->sector_size) != 0)
	{
		offset_aligned_down -= (offset % stream->sector_size);
	}

	starting_sector = offset_aligned_down / stream->sector_size;
	final_offset = offset + total;
	final_offset_aligned_up = final_offset;
	if ((final_offset % stream->sector_size) != 0)
	{
		final_offset_aligned_up += (stream->sector_size - (final_offset_aligned_up % stream->sector_size));
	}

	ending_sector = final_offset_aligned_up / stream->sector_size;
	total_bytes_aligned = final_offset_aligned_up - offset_aligned_down;
	total_sectors_to_read = total_bytes_aligned / stream->sector_size;
	if (total_sectors_to_read < 0)
	{
		panic("diskstreamer_read: total_sectors_to_read is negative");
	}

	for (int i = starting_sector; i < ending_sector; i++)
	{
		int offset_in_sector = stream->pos % stream->sector_size;
		int amount_read = stream->sector_size - (stream->pos % stream->sector_size);
		if (total < stream->sector_size)
		{
			amount_read = total;
		}

		long real_offset = disk_real_offset(stream->disk, i);
		struct disk_stream_cache_sector *cache_sector = NULL;
		int cache_res = diskstreamer_cache_find(stream->disk, real_offset, &cache_sector);
		if (cache_res < 0)
		{
			res = cache_res;
			goto out;
		}

		if (cache_res == DISK_STREAMER_CACHE_STATUS_NEW_CACHE_ENTRY)
		{
			res = disk_read_block(stream->disk, i, 1, cache_sector->buf);
			if (res < 0)
			{
				goto out;
			}
		}

		for (int j = 0; j < amount_read; j++)
		{
			*(char *)out++ = cache_sector->buf[offset_in_sector + j];
		}

		stream->pos += amount_read;
		total -= amount_read;
	}

out:
	return res;
}

void diskstreamer_close(struct disk_stream *stream)
{
	kfree(stream);
}
