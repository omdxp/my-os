#include "vector.h"
#include "status.h"
#include "memory/heap/kheap.h"
#include "memory/memory.h"
#include <stddef.h>
#include <stdint.h>

static off_t vector_offset(struct vector *vec, off_t index)
{
    return (off_t)vec->e_size * index;
}

static int vector_valid_bounds(struct vector *vec, size_t index)
{
    return (index < vec->t_elems) ? 0 : -EINVARG;
}

static void *vector_memory_at_index(struct vector *vec, size_t index)
{
    off_t offset = vector_offset(vec, index);
    return (void *)((uintptr_t)vec->memory + offset);
}

struct vector *vector_new(size_t element_size, size_t total_reserved_elements_per_resize, int flags)
{
    struct vector *vec = kpzalloc(sizeof(struct vector));
    if (!vec)
    {
        return NULL;
    }
    vec->e_size = element_size;
    vec->flags = flags;
    vec->t_elems = 0;
    vec->t_reserved_elements = total_reserved_elements_per_resize;
    vec->tm_elems = 0;
    vec->memory = NULL;

    return vec;
}

int vector_resize(struct vector *vec, size_t total_needed_elements)
{
    int res = 0;
    size_t total_elements_required = vec->t_elems + total_needed_elements;

    if (vec->tm_elems > total_elements_required)
    {
        return 0;
    }

    size_t final_total_elements_required = vec->t_reserved_elements + total_elements_required;
    size_t total_bytes_required = final_total_elements_required * vec->e_size;
    vec->memory = krealloc(vec->memory, total_bytes_required);
    if (!vec->memory)
    {
        res = -ENOMEM;
        goto out;
    }

    vec->tm_elems = final_total_elements_required;

out:
    return res;
}

int vector_push(struct vector *vec, void *elem)
{
    int res = 0;
    res = vector_resize(vec, 1);
    if (res < 0)
    {
        goto out;
    }

    res = vec->t_elems;
    memcpy(vector_memory_at_index(vec, vec->t_elems), elem, vec->e_size);
    vec->t_elems++;

out:
    return res;
}

int vector_overwrite(struct vector *vec, int index, void *elem, size_t elem_size)
{
    int res = 0;
    res = vector_valid_bounds(vec, index);
    if (res < 0)
    {
        goto out;
    }

    if (elem_size < vec->e_size)
    {
        res = -EINVARG;
        goto out;
    }

    memcpy(vector_memory_at_index(vec, index), elem, vec->e_size);

out:
    return res;
}

int vector_pop(struct vector *vec)
{
    if (vec->t_elems == 0)
    {
        return -EIO;
    }

    vec->t_elems--;
    return 0;
}

int vector_back(struct vector *vec, void *data_out, size_t size)
{
    return vector_at(vec, vec->t_elems - 1, data_out, size);
}

void vector_reorder(struct vector *vec, VECTOR_REORDER_FUNCTION reorder_function)
{
    if (!vec || !reorder_function)
        return;

    size_t count = vector_count(vec);
    if (count < 2)
        return;

    uint8_t *elem1 = kzalloc(vec->e_size);
    uint8_t *elem2 = kzalloc(vec->e_size);

    if (!elem1 || !elem2)
    {
        if (elem1)
            kfree(elem1);
        if (elem2)
            kfree(elem2);
        return;
    }

    for (size_t i = 0; i < count - 1; i++)
    {
        for (size_t j = 0; j < count - i - 1; j++)
        {
            if (vector_at(vec, j, elem1, vec->e_size) < 0)
                goto cleanup;

            if (vector_at(vec, j + 1, elem2, vec->e_size) < 0)
                goto cleanup;

            if (reorder_function(elem1, elem2) > 0)
            {
                if (vector_overwrite(vec, j, elem2, vec->e_size) < 0)
                    goto cleanup;
                if (vector_overwrite(vec, j + 1, elem1, vec->e_size) < 0)
                    goto cleanup;
            }
        }
    }

cleanup:
    kfree(elem1);
    kfree(elem2);
}

int vector_at(struct vector *vec, size_t index, void *data_out, size_t size)
{
    int res = 0;

    if (size < vec->e_size)
    {
        res = -EINVARG;
        goto out;
    }

    if (!data_out)
    {
        res = -EINVARG;
        goto out;
    }

    res = vector_valid_bounds(vec, index);
    if (res < 0)
    {
        goto out;
    }

    memcpy(data_out, vector_memory_at_index(vec, index), vec->e_size);

out:
    if (res < 0)
    {
        memset(data_out, 0, size);
    }
    return res;
}

int vector_pop_element(struct vector *vec, void *mem_val, size_t size)
{
    int res = 0;

    if (size != vec->e_size)
    {
        return -EINVARG;
    }

    size_t total_elements = vector_count(vec);
    long index_to_remove = -1;

    void *tmp_mem = kzalloc(size);
    if (!tmp_mem)
    {
        return -ENOMEM;
    }

    for (size_t i = 0; i < total_elements; i++)
    {
        res = vector_at(vec, i, tmp_mem, size);
        if (res < 0)
        {
            goto out;
        }

        if (memcmp(tmp_mem, mem_val, size) == 0)
        {
            index_to_remove = i;
            break;
        }
    }

    if (index_to_remove == -1)
    {
        res = -EIO;
        goto out;
    }

    if ((size_t)index_to_remove == total_elements - 1)
    {
        res = vector_pop(vec);
        goto out;
    }

    {
        uint8_t *ptr_to_element_to_delete = (uint8_t *)vec->memory + (index_to_remove * size);
        uint8_t *ptr_to_next_element = ptr_to_element_to_delete + size;

        size_t total_elements_to_copy = (vec->t_elems - index_to_remove) - 1;
        size_t total_bytes_to_copy = total_elements_to_copy * size;

        memcpy(ptr_to_element_to_delete, ptr_to_next_element, total_bytes_to_copy);
        vec->t_elems--;
    }

out:
    kfree(tmp_mem);
    return res;
}

size_t vector_count(struct vector *vec)
{
    return vec->t_elems;
}

void vector_free(struct vector *vec)
{
    if (vec->memory)
    {
        kfree(vec->memory);
    }
    kfree(vec);
}

int vector_grow(struct vector *vec, size_t additional_elements)
{
    int res = 0;
    res = vector_resize(vec, additional_elements);
    if (res < 0)
    {
        goto out;
    }

    vec->t_elems += additional_elements;

out:
    return res;
}