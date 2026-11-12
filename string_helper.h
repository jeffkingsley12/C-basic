#ifndef STRING_HELPER_H
#define STRING_HELPER_H

#include <stddef.h>
#include "vector.h"

/**
 * @brief Checks whether a positive integer is an exact power of two.
 */
#define IS_POWER_OF_TWO(A) ((size_t)(A) != 0 && (((size_t)(A) & ((size_t)(A) - 1)) == 0))

/**
 * @brief Aligns a size upwards to the nearest multiple of a power-of-two alignment boundary.
 * @note Uses bitwise manipulation: (size + (A - 1)) & ~(A - 1)
 */
#define ALIGN_UP(size, A) (((size_t)(size) + ((size_t)(A) - 1)) & ~((size_t)(A) - 1))

/**
 * @brief Allocates an exactly-sized, padded string bucket routed through the vector's unified Allocator.
 * @param src The source null-terminated C string.
 * @param alignment The target byte boundary alignment (must be a power of two, e.g., 8, 16).
 * @param allocator Pointer to the unified subsystem Allocator context block.
 * @return Pointer to the allocated padded string payload, or NULL on OOM or error.
 */
char* create_padded_string_impl(const char *src, size_t alignment, Allocator *allocator);

#endif // STRING_HELPER_H
