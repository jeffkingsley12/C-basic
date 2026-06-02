#ifndef ARRAY_H
#define ARRAY_H

#include <stddef.h>
#include <string.h>

/* Forward declarations of custom allocator architecture */
typedef struct Allocator Allocator;

/**
 * AllocatorFunc: Unified allocation/reallocation function pointer
 * 
 * @param ptr     Existing memory block (NULL for fresh allocation)
 * @param size    New size requested (0 to free)
 * @param context Allocator-specific context
 * 
 * Returns: New pointer, or NULL on failure (existing block unchanged on realloc failure)
 */
typedef void* (*AllocatorFunc)(void *ptr, size_t size, void *context);

struct Allocator {
    AllocatorFunc alloc;
    void *context;
};

/**
 * Array_Header: Metadata stored immediately before array payload
 * 
 * Layout in memory:
 *   [Array_Header (32 bytes)] [Data payload (N * item_size)]
 *                            ^
 *                    Returned pointer points here
 * 
 * The 32-byte size ensures 16-byte alignment of payload when allocated
 * on standard 16-byte aligned allocators (required for SIMD types).
 */
typedef struct {
    size_t capacity;       /* 8 bytes */
    size_t length;         /* 8 bytes */
    size_t item_size;      /* 8 bytes - cached for realloc safety */
    Allocator *allocator;  /* 8 bytes */
} Array_Header;            /* Total: 32 bytes (perfectly aligned) */

#define ARRAY_INITIAL_CAPACITY 4

/**
 * array(T, allocator_ptr)
 * 
 * Create a new dynamically-allocated array of type T.
 * 
 * Usage:
 *   Allocator my_alloc = {...};
 *   int *numbers = array(int, &my_alloc);
 *   array_push(numbers, 42);
 *   array_free(numbers);
 */
#define array(T, allocator_ptr) \
    (T *)array_init(sizeof(T), ARRAY_INITIAL_CAPACITY, (allocator_ptr))

/**
 * array_free(a)
 * 
 * Free the array and set pointer to NULL.
 * Safe to call on NULL pointers.
 * 
 * Usage:
 *   array_free(my_array);  // my_array is now NULL
 */
#define array_free(a) do {          \
    if (a) {                        \
        array_free_impl(a);         \
        (a) = NULL;                 \
    }                               \
} while(0)

/**
 * array_push(a, value)
 * 
 * Append value to the end of array, expanding capacity if needed.
 * Handles NULL arrays gracefully (no-op if allocation fails).
 * 
 * CAUTION: Evaluates (a) multiple times. For complex expressions,
 * store in a temporary variable first:
 *   int *temp = my_array;
 *   array_push(temp, value);
 *   my_array = temp;
 * 
 * Usage:
 *   int *arr = array(int, &alloc);
 *   array_push(arr, 10);
 *   array_push(arr, 20);
 */
#define array_push(a, value) do {                                            \
    (a) = array_ensure_capacity((a), sizeof(*(a)));                          \
    if ((a) != NULL) {                                                       \
        Array_Header *_h = (Array_Header *)(a) - 1;                         \
        (a)[_h->length++] = (value);                                         \
    }                                                                        \
} while(0)

/**
 * array_pop(a, out_value)
 * 
 * Remove and retrieve the last element.
 * Does nothing if array is NULL or empty.
 * 
 * Usage:
 *   int val;
 *   array_pop(arr, val);
 */
#define array_pop(a, out_value) do {                                         \
    if ((a) && array_length(a) > 0) {                                        \
        Array_Header *_h = (Array_Header *)(a) - 1;                         \
        (out_value) = (a)[--_h->length];                                     \
    }                                                                        \
} while(0)

/**
 * array_length(a)
 * 
 * Get the current number of elements in the array.
 * Safe to call on NULL (returns 0).
 * 
 * Usage:
 *   for (size_t i = 0; i < array_length(arr); i++) { ... }
 */
#define array_length(a) \
    ((a) ? ((Array_Header *)(a) - 1)->length : 0)

/**
 * array_capacity(a)
 * 
 * Get the allocated capacity (total slots available).
 * Safe to call on NULL (returns 0).
 * 
 * Usage:
 *   if (array_length(arr) == array_capacity(arr)) { ... } // full
 */
#define array_capacity(a) \
    ((a) ? ((Array_Header *)(a) - 1)->capacity : 0)

/**
 * array_clear(a)
 * 
 * Reset array to empty without freeing memory.
 * Capacity is preserved for reuse.
 * 
 * Usage:
 *   array_clear(arr);  // length becomes 0, capacity unchanged
 */
#define array_clear(a) do {                                                  \
    if (a) {                                                                 \
        ((Array_Header *)(a) - 1)->length = 0;                              \
    }                                                                        \
} while(0)

/* ========================================================================
 * Core Implementation Functions (Called through macros)
 * ======================================================================== */

/**
 * array_init(item_size, capacity, allocator)
 * 
 * Internal: Initialize a new array with given capacity.
 * Called by array(T, allocator) macro.
 */
void *array_init(size_t item_size, size_t capacity, Allocator *allocator);

/**
 * array_free_impl(arr)
 * 
 * Internal: Free array memory and metadata.
 * Called by array_free(a) macro.
 */
void array_free_impl(void *arr);

/**
 * array_ensure_capacity(arr, item_size)
 * 
 * Internal: Grow array capacity if at limit (geometric growth: 2x).
 * Returns updated pointer (may differ from input if reallocated).
 * Returns NULL on allocation failure (original array unchanged).
 * Called by array_push(a, value) macro.
 */
void *array_ensure_capacity(void *arr, size_t item_size);

#endif /* ARRAY_H */
