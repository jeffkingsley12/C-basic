#ifndef STRING_HELPER_H
#define STRING_HELPER_H

#include <stddef.h>
#include "ce_types.h"
#include "vector.h"

#define IS_POWER_OF_TWO(A) ((size_t)(A) != 0 && (((size_t)(A) & ((size_t)(A) - 1)) == 0))
#define ALIGN_UP(size, A) (((size_t)(size) + ((size_t)(A) - 1)) & ~((size_t)(A) - 1))

char *create_padded_string_impl(const char *src, size_t alignment, Allocator *allocator);

#endif /* STRING_HELPER_H */
