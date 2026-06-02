#ifndef ARRAY_H
#define ARRAY_H

#include <stddef.h>

// Forward declarations of your custom allocator architecture
typedef struct Allocator Allocator;
typedef void* (*AllocatorFunc)(size_t size, void *context);

struct Allocator {
    AllocatorFunc alloc;
    void *context;
};

// Metadata Header preceding the data array payload in memory
typedef struct {
    size_t capacity;
    size_t length;
    Allocator *a;
} Array_Header;

#define ARRAY_INITIAL_CAPACITY 4

// Macro API
#define array(T, allocator) \
    (T *)array_init(sizeof(T), ARRAY_INITIAL_CAPACITY, (allocator))

#define array_free(a) do {          \
    if (a) {                        \
        array_free_impl(a);         \
        (a) = NULL;                 \
    }                               \
} while(0)

#define array_push(a, value) do {                                           \
    (a) = array_ensure_capacity((a), sizeof(*(a)));                         \
    if ((a) != NULL) {                                                      \
        ((Array_Header *)(a) - 1)->length++;                                \
        (a)[((Array_Header *)(a) - 1)->length - 1] = (value);               \
    }                               \
} while(0)

#define array_length(a)   ((a) ? ((Array_Header *)(a) - 1)->length   : 0)
#define array_capacity(a) ((a) ? ((Array_Header *)(a) - 1)->capacity : 0)

// Core Implementation Functions (Hidden behind macros)
void *array_init(size_t item_size, size_t capacity, Allocator *a);
void  array_free_impl(void *arr);
void *array_ensure_capacity(void *arr, size_t item_size);

#endif // ARRAY_H
