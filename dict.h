#ifndef VECTOR_H
#define VECTOR_H

#include <stddef.h>
#include <string.h>
#include <stdalign.h>
#include "ce_types.h"
#include "allocator.h"

#define VECTOR_INITIAL_CAPACITY 4

typedef struct {
    alignas(max_align_t) size_t capacity;
    size_t length;
    size_t item_size;
    Allocator *allocator;
} Vector_Header;

_Static_assert(sizeof(Vector_Header) % alignof(max_align_t) == 0,
               "Vector_Header must preserve max_align_t payload alignment");

#define vector_hdr_(v) ((Vector_Header *)(v) - 1)
void *vector_init_impl(size_t item_size, size_t capacity, Allocator *allocator);
void vector_free_impl(void *arr);
void *vector_ensure_capacity_impl(void *arr);
#define vector(T, allocator_ptr) ((T *)vector_init_impl(sizeof(T), VECTOR_INITIAL_CAPACITY, (allocator_ptr)))
#define vector_length(v) ((v) ? vector_hdr_(v)->length : (size_t)0)
#define vector_capacity(v) ((v) ? vector_hdr_(v)->capacity : (size_t)0)
#define vector_clear(v) do { void *_v = (v); if (_v) vector_hdr_(_v)->length = 0; } while (0)
#define vector_truncate(v, n) do { \
    void *_v = (v); size_t _n = (size_t)(n); \
    if (_v && _n <= vector_hdr_(_v)->length) vector_hdr_(_v)->length = _n; \
} while (0)
#define vector_free(v) do { if (v) { vector_free_impl(v); (v) = NULL; } } while (0)

#if defined(__GNUC__) || defined(__clang__)
#define vector_push(v, value) ({ \
    __typeof__(v) *_ref = &(v); \
    __typeof__(**_ref) _value = (value); \
    int _ok = 0; \
    if (*_ref) { \
        void *_moved = vector_ensure_capacity_impl(*_ref); \
        if (_moved) *_ref = (__typeof__(v))_moved; \
        Vector_Header *_h = vector_hdr_(*_ref); \
        if (_h->length < _h->capacity) { (*_ref)[_h->length++] = _value; _ok = 1; } \
    } \
    _ok; \
})
#define vector_pop(v, out) ({ \
    __typeof__(v) _vec = (v); int _ok = 0; \
    if (_vec && vector_hdr_(_vec)->length) { (out) = _vec[--vector_hdr_(_vec)->length]; _ok = 1; } \
    _ok; \
})
#define vector_insert(v, index, value) ({ \
    __typeof__(v) *_ref = &(v); size_t _idx = (size_t)(index); int _ok = 0; \
    if (*_ref && _idx <= vector_hdr_(*_ref)->length) { \
        __typeof__(**_ref) _value = (value); \
        void *_moved = vector_ensure_capacity_impl(*_ref); if (_moved) *_ref = (__typeof__(v))_moved; \
        Vector_Header *_h = vector_hdr_(*_ref); \
        if (_h->length < _h->capacity) { \
            char *_base = (char *)*_ref; size_t _sz = _h->item_size; \
            if (_idx < _h->length) memmove(_base + (_idx + 1) * _sz, _base + _idx * _sz, (_h->length - _idx) * _sz); \
            (*_ref)[_idx] = _value; _h->length++; _ok = 1; \
        } \
    } \
    _ok; \
})
#define vector_remove_at(v, index) ({ \
    __typeof__(v) _vec = (v); size_t _idx = (size_t)(index); int _ok = 0; \
    if (_vec && _idx < vector_hdr_(_vec)->length) { \
        Vector_Header *_h = vector_hdr_(_vec); char *_base = (char *)_vec; size_t _sz = _h->item_size; \
        _h->length--; if (_idx < _h->length) memmove(_base + _idx * _sz, _base + (_idx + 1) * _sz, (_h->length - _idx) * _sz); _ok = 1; \
    } \
    _ok; \
})
#else
#define vector_push(v, value, out_success) do { \
    void *_ptr = (v); *(out_success) = 0; \
    if (_ptr) { void *_moved = vector_ensure_capacity_impl(_ptr); \
        if (_moved) { if (_moved != _ptr) (v) = _moved; _ptr = _moved; } \
        Vector_Header *_h = vector_hdr_(_ptr); \
        if (_h->length < _h->capacity) { (v)[_h->length++] = (value); *(out_success) = 1; } \
    } \
} while (0)
#define vector_pop(v, out, out_success) do { \
    void *_ptr = (v); *(out_success) = 0; \
    if (_ptr) { Vector_Header *_h = vector_hdr_(_ptr); \
        if (_h->length) { --_h->length; (out) = (v)[_h->length]; *(out_success) = 1; } \
    } \
} while (0)
#define vector_insert(v, index, value, out_success) do { \
    void *_ptr = (v); size_t _idx = (size_t)(index); *(out_success) = 0; \
    if (_ptr && _idx <= vector_hdr_(_ptr)->length) { \
        void *_moved = vector_ensure_capacity_impl(_ptr); if (_moved) { if (_moved != _ptr) (v) = _moved; _ptr = _moved; } \
        Vector_Header *_h = vector_hdr_(_ptr); \
        if (_h->length < _h->capacity) { char *_base = (char *)(v); size_t _sz = _h->item_size; \
            if (_idx < _h->length) memmove(_base + (_idx + 1) * _sz, _base + _idx * _sz, (_h->length - _idx) * _sz); \
            (v)[_idx] = (value); _h->length++; *(out_success) = 1; \
        } \
    } \
} while (0)
#define vector_remove_at(v, index, out_success) do { \
    void *_ptr = (v); size_t _idx = (size_t)(index); *(out_success) = 0; \
    if (_ptr && _idx < vector_hdr_(_ptr)->length) { Vector_Header *_h = vector_hdr_(_ptr); char *_base = (char *)_ptr; size_t _sz = _h->item_size; \
        _h->length--; if (_idx < _h->length) memmove(_base + _idx * _sz, _base + (_idx + 1) * _sz, (_h->length - _idx) * _sz); *(out_success) = 1; \
    } \
} while (0)
#endif

#endif /* VECTOR_H */
