#ifndef TEST_ALLOCATOR_H
#define TEST_ALLOCATOR_H

#include "vector.h"

typedef struct {
    size_t active_allocations;
    size_t total_allocations_built;
    size_t fail_after_n_allocations; // 0 means "never fail automatically"
    int force_immediate_failure;     // Toggle to instantly trigger OOM
} Mock_Allocator_Context;

// Implementation of the AllocatorFunc interface
void* mock_allocator_bridge(void *ptr, size_t size, void *context);

#endif
