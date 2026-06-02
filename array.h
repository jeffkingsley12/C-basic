#ifndef ARRAY_H
#define ARRAY_H

#include <stddef.h>
#include <string.h>

typedef struct Allocator Allocator;

// Realloc-style allocator signature.
// CONTRACT: If ptr != NULL and size > 0, and allocation fails, the allocator 
// MUST return NULL and leave the original ptr valid and untouched.
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
    max_align_t _align;    // Guarantees strict alignment for SIMD/over-aligned payloads
} Array_Header;            

#define ARRAY_INITIAL_CAPACITY 4

#define array_hdr_(a) ((Array_Header *)(a) - 1)

#define array(T, allocator_ptr) \
    (T *)array_init(sizeof(T), ARRAY_INITIAL_CAPACITY, (allocator_ptr))

#define array_free(a) do {          \
    if (a) {                        \
        array_free_impl(a);         \
        (a) = NULL;                 \
    }                               \
} while(0)

/* ========================================================================
 * MACRO IMPLEMENTATION
 * ======================================================================== */
#if defined(__GNUC__) || defined(__clang__)

/* GCC/Clang Implementation: Uses statement expressions and typeof for perfect safety */
#define array_push(a, value) ({                                             \
    __typeof__(a) *_ptr_ref = &(a);                                         \
    void *_moved = array_ensure_capacity(*_ptr_ref);                        \
    if (_moved != NULL) *_ptr_ref = _moved;                                 \
    int _success = (*_ptr_ref != NULL &&                                    \
                    array_hdr_(*_ptr_ref)->length < array_hdr_(*_ptr_ref)->capacity); \
    if (_success) (*_ptr_ref)[array_hdr_(*_ptr_ref)->length++] = (value);   \
    _success;                                                               \
})

#define array_pop(a, out_value) ({                                          \
    __typeof__(a) _arr = (a);                                               \
    int _popped = 0;                                                        \
    if (_arr && array_hdr_(_arr)->length > 0) {                             \
        (out_value) = _arr[--array_hdr_(_arr)->length];                     \
        _popped = 1;                                                        \
    }                                                                       \
    _popped;                                                                \
})

#else

/* Standard C Fallback (MSVC, etc.) 
 * CONTRACT: 'a' must be a simple lvalue without side-effects (e.g., cannot be arr[i++]) */
#define array_push(a, value) do {                                           \
    void *_target_ptr = (a);                                                \
    if (_target_ptr) {                                                      \
        void *_moved = array_ensure_capacity(_target_ptr);                  \
        if (_moved != NULL) {                                               \
            (a) = _moved; /* Unavoidable lvalue re-evaluation in Std C */   \
            _target_ptr = _moved;                                           \
        }                                                                   \
        Array_Header *_h = array_hdr_(_target_ptr);                         \
        if (_h->length < _h->capacity) {                                    \
            (a)[_h->length++] = (value);                                    \
        }                                                                   \
    }                                                                       \
} while(0)

#define array_pop(a, out_value) do {                                        \
    void *_arr_pop = (a);                                                   \
    if (_arr_pop) {                                                         \
        Array_Header *_h = array_hdr_(_arr_pop);                            \
        if (_h->length > 0) {                                               \
            _h->length--;                                                   \
            /* Strictly Standard C: Use memcpy to avoid void* math and typeof */ \
            memcpy(&(out_value),                                            \
                   (char*)_arr_pop + (_h->length * _h->item_size),          \
                   _h->item_size);                                          \
        }                                                                   \
    }                                                                       \
} while(0)

#endif

#define array_length(a)   ((a) ? array_hdr_(a)->length   : 0)
#define array_capacity(a) ((a) ? array_hdr_(a)->capacity : 0)

#define array_clear(a) do {                                                 \
    if (a) array_hdr_(a)->length = 0;                                       \
} while(0)

void *array_init(size_t item_size, size_t capacity, Allocator *allocator);
void  array_free_impl(void *arr);
void *array_ensure_capacity(void *arr);

#endif /* ARRAY_H */
