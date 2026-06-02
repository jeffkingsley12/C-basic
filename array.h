#ifndef ARRAY_H
#define ARRAY_H

#include <stddef.h>
#include <string.h>

typedef struct Allocator Allocator;

typedef void* (*AllocatorFunc)(void *ptr, size_t size, void *context);

struct Allocator {
    AllocatorFunc alloc;
    void *context;
};

typedef struct {
    size_t capacity;       
    size_t length;         
    size_t item_size;      
    Allocator *allocator;  
} Array_Header;            

#define ARRAY_INITIAL_CAPACITY 4

/* Helper macro to safely access the header block */
#define __array_hdr(a) ((Array_Header *)(a) - 1)

#define array(T, allocator_ptr) \
    (T *)array_init(sizeof(T), ARRAY_INITIAL_CAPACITY, (allocator_ptr))

#define array_free(a) do {          \
    if (a) {                        \
        array_free_impl(a);         \
        (a) = NULL;                 \
    }                               \
} while(0)

/* ========================================================================
 * MACRO IMPLEMENTATION WITH DOUBLE-EVALUATION SAFEGUARDS
 * ======================================================================== */
#if defined(__GNUC__) || defined(__clang__)

/* GCC/Clang Implementation: Evaluates arguments exactly once */
#define array_push(a, value) ({                                             \
    __typeof__(a) *_ptr_ref = &(a);                                         \
    void *_moved = array_ensure_capacity(*_ptr_ref, sizeof(**_ptr_ref));    \
    int _success = (_moved != NULL);                                        \
    if (_success) {                                                         \
        *_ptr_ref = _moved;                                                 \
        Array_Header *_h = __array_hdr(*_ptr_ref);                          \
        (*_ptr_ref)[_h->length++] = (value);                                \
    }                                                                       \
    _success;                                                               \
})

#define array_pop(a, out_value) ({                                          \
    __typeof__(a) _arr = (a);                                               \
    int _popped = 0;                                                        \
    if (_arr && __array_hdr(_arr)->length > 0) {                            \
        (out_value) = _arr[--__array_hdr(_arr)->length];                    \
        _popped = 1;                                                        \
    }                                                                       \
    _popped;                                                                \
})

#else

/* Standard C Fallback: Explicit warning for multiple argument evaluation */
#define array_push(a, value) do {                                           \
    void *_target_ptr = (a);                                                \
    if (_target_ptr) {                                                      \
        void *_moved = array_ensure_capacity(_target_ptr, sizeof(*(a)));    \
        if (_moved) {                                                       \
            (a) = _moved;                                                   \
            (a)[__array_hdr(a)->length++] = (value);                        \
        }                                                                   \
    }                                                                       \
} while(0)

#define array_pop(a, out_value) do {                                         \
    if ((a) && __array_hdr(a)->length > 0) {                                \
        (out_value) = (a)[--__array_hdr(a)->length];                        \
    }                                                                       \
} while(0)

#endif

#define array_length(a)   ((a) ? __array_hdr(a)->length   : 0)
#define array_capacity(a) ((a) ? __array_hdr(a)->capacity : 0)

#define array_clear(a) do {                                                 \
    if (a) {                                                                \
        __array_hdr(a)->length = 0;                                         \
    }                                                                       \
} while(0)

void *array_init(size_t item_size, size_t capacity, Allocator *allocator);
void  array_free_impl(void *arr);
void *array_ensure_capacity(void *arr, size_t item_size);

#endif /* ARRAY_H */
