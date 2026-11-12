#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>
#include "ce_types.h"

typedef void *(*AllocatorFunc)(void *ptr, size_t size, void *context);

typedef struct Allocator {
    AllocatorFunc alloc;
    void *context;
} Allocator;

#endif /* ALLOCATOR_H */
