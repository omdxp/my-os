#pragma once
#include <stddef.h>
#include <stdint.h>
#include "types.h"

typedef int (*VECTOR_REORDER_FUNCTION)(void *first_element, void *second_element);

enum
{
    VECTOR_NO_FLAGS = 0
};

struct vector
{
    void *memory;               // Vector data storage
    int flags;                  // Flags associated with the vector
    size_t e_size;              // Size of a single element
    size_t t_elems;             // Total elements currently in vector
    size_t tm_elems;            // Total capacity (max elements before resize needed)
    size_t t_reserved_elements; // Elements to reserve per resize
};

/**
 * Creates a new vector
 * @param element_size The size of each element
 * @param total_reserved_elements_per_resize Number of elements to reserve when resizing
 * @param flags Vector flags
 * @return Pointer to the new vector or NULL on failure
 */
struct vector *vector_new(size_t element_size, size_t total_reserved_elements_per_resize, int flags);

/**
 * Frees the vector memory
 */
void vector_free(struct vector *vec);

/**
 * Adds an element to the vector
 * @param vec The vector to push to
 * @param elem The element to add
 * @return Index of the new element or negative on error
 */
int vector_push(struct vector *vec, void *elem);

/**
 * Removes the last element from the vector
 * @param vec The vector to pop from
 * @return 0 on success, negative on error
 */
int vector_pop(struct vector *vec);

/**
 * Reorders the vector using the provided comparison function
 */
void vector_reorder(struct vector *vec, VECTOR_REORDER_FUNCTION reorder_function);

/**
 * Overwrites an element at the given index
 * @param vec The vector to write to
 * @param index The index to overwrite
 * @param elem The element to write
 * @param elem_size The element size (must match vector element size)
 * @return 0 on success, negative on error
 */
int vector_overwrite(struct vector *vec, int index, void *elem, size_t elem_size);

/**
 * Retrieves the last element
 * @param vec The vector
 * @param data_out Buffer to write the element to
 * @param size Expected element size
 * @return 0 on success, negative on error
 */
int vector_back(struct vector *vec, void *data_out, size_t size);

/**
 * Retrieves an element at the given index
 * @param vec The vector
 * @param index The index to retrieve
 * @param data_out Buffer to write the element to
 * @param size Expected element size
 * @return 0 on success, negative on error
 */
int vector_at(struct vector *vec, size_t index, void *data_out, size_t size);

/**
 * Returns the total number of elements
 * @param vec The vector
 * @return Element count
 */
size_t vector_count(struct vector *vec);

/**
 * Removes the first element matching the given value
 * @param vec The vector
 * @param mem_val Pointer to the value to remove
 * @param size The element size
 * @return 0 on success, negative on error
 */
int vector_pop_element(struct vector *vec, void *mem_val, size_t size);
