/**
 * @file vector.h
 * @brief High-Performance, Type-Safe, Data-Oriented Dynamic Vector for C.
 *
 * @section ARCHITECTURE (V4 Layout - Intrusive Header Pattern)
 * This library implements a data-oriented dynamic array (vector) that mimics raw 
 * hardware vectors. Instead of using a standard wrapper struct that points to an 
 * allocated buffer (which introduces pointer chasing and cache misses), this design 
 * embeds metadata in an "intrusive header" immediately preceding the raw payload.
 *
 * Memory Layout:
 * +---------------------------------------------------------------+---------------------------------+
 * | Capacity (alignas max_align_t) | Length | Item Size | Allocator | Element 0 | Element 1 | ...     |
 * +---------------------------------------------------------------+---------------------------------+
 * \__________________________ Array_Header (Hidden) ______________/ \______ User Pointer (v) ________/
 * (16-Byte/SIMD Aligned)
 *
 * The user-facing pointer `v` points directly to Element 0. This allows standard
 * array subscript syntax (e.g., `v[i]`) and triggers the CPU's hardware data 
 * prefetcher for maximum sequential iteration speed ($O(1)$ sequential reads).
 *
 * @section DESIGN_GUARANTEES
 * - Memory Efficiency: Header and payload are allocated as a single contiguous 
 * heap block (exactly 1 allocation per vector instance).
 * - Compile-Time Alignment: Leverages C11 alignas(max_align_t) and `_Static_assert`
 * to guarantee that the header preserves strict alignment boundaries for SIMD types
 * with zero padding waste.
 * - Out-Of-Memory (OOM) Safety: Reallocation sequences follow an anti-clobber pattern. 
 * If the backing allocator runs out of memory during a resize, the vector signals 
 * failure, leaves original elements uncorrupted, and preserves the user's pointer.
 */

#ifndef VECTOR_H
#define VECTOR_H

#include <stddef.h>
#include <string.h>
#include <stdalign.h>

#define VECTOR_INITIAL_CAPACITY 4

typedef struct Allocator Allocator;

/**
 * @brief Realloc-style allocator signature.
 * @note CONTRACT: If ptr != NULL and size > 0, and allocation fails, the allocator 
 * MUST return NULL and leave the original ptr valid and untouched.
 * If size == 0, it must act as a free() operation and return NULL.
 */
typedef void* (*AllocatorFunc)(void *ptr, size_t size, void *context);

struct Allocator {
    AllocatorFunc alloc;
    void *context;
};

/* Use C11 alignas(max_align_t) to guarantee SIMD/max alignment
 * without wasting dummy alignment value fields. */
typedef struct {
    alignas(max_align_t) size_t capacity;       
    size_t length;         
    size_t item_size;      
    Allocator *allocator;  
} Vector_Header;

_Static_assert(
    sizeof(Vector_Header) % alignof(max_align_t) == 0, 
    "Vector_Header must be strictly aligned to max_align_t for safe SIMD/payload alignment"
);

#define vector_hdr_(v) ((Vector_Header *)(v) - 1)

/* Core Runtime Engines */
void *vector_init_impl(size_t item_size, size_t capacity, Allocator *allocator);
void  vector_free_impl(void *arr);
void *vector_ensure_capacity_impl(void *arr);

/**
 * @brief Initializes a type-safe vector.
 * @param T The data type (e.g., int, MyStruct).
 * @param allocator_ptr Pointer to an Allocator configuration instance.
 */
#define vector(T, allocator_ptr) \
    ((T *)vector_init_impl(sizeof(T), VECTOR_INITIAL_CAPACITY, (allocator_ptr)))

#define vector_length(v)   ((v) ? vector_hdr_(v)->length   : (size_t)0)
#define vector_capacity(v) ((v) ? vector_hdr_(v)->capacity : (size_t)0)

#define vector_clear(v) do { \
    void *_v_clr = (v); \
    if (_v_clr) vector_hdr_(_v_clr)->length = 0; \
} while(0)

/** @brief Shrinks length to n. No-op if v is NULL or n > current length (never grows). */
#define vector_truncate(v, n) do { \
    void *_v_trunc = (v); \
    size_t _n_trunc = (size_t)(n); \
    if (_v_trunc && _n_trunc <= vector_hdr_(_v_trunc)->length) { \
        vector_hdr_(_v_trunc)->length = _n_trunc; \
    } \
} while(0)

#define vector_free(v) do { \
    if (v) { \
        vector_free_impl(v); \
        (v) = NULL; \
    } \
} while(0)

/* ========================================================================
 * EXTENDED MACRO INTERFACE (COMPILER SPECIATION)
 * ======================================================================== */
#if defined(__GNUC__) || defined(__clang__)

/* GCC/Clang Implementation: Employs statement expressions and __typeof__ for absolute safety */
#define vector_push(v, value) ({ \
    __typeof__(v) *_ptr_ref = &(v); \
    __typeof__(**_ptr_ref) _val_copy = (value); \
    int _success = 0; \
    if (*_ptr_ref) { \
        void *_moved = vector_ensure_capacity_impl(*_ptr_ref); \
        if (_moved != NULL) *_ptr_ref = (__typeof__(v))_moved; \
        Vector_Header *_h = vector_hdr_(*_ptr_ref); \
        if (_h->length < _h->capacity) { \
            (*_ptr_ref)[_h->length++] = _val_copy; \
            _success = 1; \
        } \
    } \
    _success; \
})

#define vector_pop(v, out_value) ({ \
    __typeof__(v) _vec = (v); \
    int _popped = 0; \
    if (_vec && vector_hdr_(_vec)->length > 0) { \
        (out_value) = _vec[--vector_hdr_(_vec)->length]; \
        _popped = 1; \
    } \
    _popped; \
})

/**
 * @brief Inserts an element at an arbitrary index, shifting subsequent elements right.
 * @return 1 on success, 0 on failure (OOM or out-of-bounds).
 */
#define vector_insert(v, index, value) ({ \
    __typeof__(v) *_ptr_ref = &(v); \
    size_t _idx = (size_t)(index); \
    int _success = 0; \
    if (*_ptr_ref && _idx <= vector_hdr_(*_ptr_ref)->length) { \
        __typeof__(**_ptr_ref) _val_copy = (value); \
        void *_moved = vector_ensure_capacity_impl(*_ptr_ref); \
        if (_moved != NULL) *_ptr_ref = (__typeof__(v))_moved; \
        Vector_Header *_h = vector_hdr_(*_ptr_ref); \
        if (_h->length < _h->capacity) { \
            char *_base = (char*)(*_ptr_ref); \
            size_t _sz = _h->item_size; \
            if (_idx < _h->length) { \
                memmove(_base + ((_idx + 1) * _sz), _base + (_idx * _sz), (_h->length - _idx) * _sz); \
            } \
            (*_ptr_ref)[_idx] = _val_copy; \
            _h->length++; \
            _success = 1; \
        } \
    } \
    _success; \
})

/**
 * @brief Removes an element at an arbitrary index, shifting subsequent elements left.
 * @return 1 on success, 0 on failure (out-of-bounds or empty vector).
 */
#define vector_remove_at(v, index) ({ \
    __typeof__(v) _vec = (v); \
    size_t _idx = (size_t)(index); \
    int _success = 0; \
    if (_vec && _idx < vector_hdr_(_vec)->length) { \
        Vector_Header *_h = vector_hdr_(_vec); \
        char *_base = (char*)_vec; \
        size_t _sz = _h->item_size; \
        _h->length--; \
        if (_idx < _h->length) { \
            memmove(_base + (_idx * _sz), _base + ((_idx + 1) * _sz), (_h->length - _idx) * _sz); \
        } \
        _success = 1; \
    } \
    _success; \
})

#else

/* Standard C Fallback (MSVC, etc.) 
 * CONTRACT: 'v' must be a stable l-value variable. Out parameters pass state back safely. */
#define vector_push(v, value, out_success) do { \
    void *_target_ptr = (v); \
    *(out_success) = 0; \
    if (_target_ptr) { \
        void *_moved = vector_ensure_capacity_impl(_target_ptr); \
        if (_moved != NULL) { \
            if (_moved != _target_ptr) (v) = _moved; \
            _target_ptr = _moved; \
        } \
        Vector_Header *_h = vector_hdr_(_target_ptr); \
        if (_h->length < _h->capacity) { \
            (v)[_h->length++] = (value); \
            *(out_success) = 1; \
        } \
    } \
} while(0)

#define vector_pop(v, out_value, out_success) do { \
    void *_vec_pop = (v); \
    *(out_success) = 0; \
    if (_vec_pop) { \
        Vector_Header *_h = vector_hdr_(_vec_pop); \
        if (_h->length > 0) { \
            _h->length--; \
            (out_value) = (v)[_h->length]; \
            *(out_success) = 1; \
        } \
    } \
} while(0)

#define vector_insert(v, index, value, out_success) do { \
    void *_target_ptr = (v); \
    size_t _idx = (size_t)(index); \
    *(out_success) = 0; \
    if (_target_ptr && _idx <= vector_hdr_(_target_ptr)->length) { \
        void *_moved = vector_ensure_capacity_impl(_target_ptr); \
        if (_moved != NULL) { \
            if (_moved != _target_ptr) (v) = _moved; \
            _target_ptr = _moved; \
        } \
        Vector_Header *_h = vector_hdr_(_target_ptr); \
        if (_h->length < _h->capacity) { \
            char *_base = (char*)(v); \
            size_t _sz = _h->item_size; \
            if (_idx < _h->length) { \
                memmove(_base + ((_idx + 1) * _sz), _base + (_idx * _sz), (_h->length - _idx) * _sz); \
            } \
            (v)[_idx] = (value); \
            _h->length++; \
            *(out_success) = 1; \
        } \
    } \
} while(0)

#define vector_remove_at(v, index, out_success) do { \
    void *_vec_rem = (v); \
    size_t _idx = (size_t)(index); \
    *(out_success) = 0; \
    if (_vec_rem && _idx < vector_hdr_(_vec_rem)->length) { \
        Vector_Header *_h = vector_hdr_(_vec_rem); \
        char *_base = (char*)_vec_rem; \
        size_t _sz = _h->item_size; \
        _h->length--; \
        if (_idx < _h->length) { \
            memmove(_base + (_idx * _sz), _base + ((_idx + 1) * _sz), (_h->length - _idx) * _sz); \
        } \
        *(out_success) = 1; \
    } \
} while(0)

#endif

#endif // VECTOR_H
