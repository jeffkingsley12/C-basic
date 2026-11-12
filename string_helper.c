#include "string_helper.h"
#include <string.h>
#include <stdint.h>

char* create_padded_string_impl(const char *src, size_t alignment, Allocator *allocator) {
    if (!src || !allocator || !allocator->alloc) return NULL;
    if (!IS_POWER_OF_TWO(alignment)) return NULL;

    size_t raw_len = strlen(src);
    if (raw_len == SIZE_MAX) return NULL;
    size_t str_len = raw_len + 1; // Count characters + implicit null terminator

    if (str_len > SIZE_MAX - (alignment - 1)) return NULL;
    size_t padded_size = ALIGN_UP(str_len, alignment);

    char *padded_slot = (char*)allocator->alloc(NULL, padded_size, allocator->context);
    if (!padded_slot) return NULL;

    memcpy(padded_slot, src, str_len);
    if (padded_size > str_len) {
        memset(padded_slot + str_len, 0, padded_size - str_len);
    }

    return padded_slot;
}
