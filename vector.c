#include "vector.h"
#include <stdlib.h>
#include <stdint.h>

void *vector_init_impl(size_t item_size, size_t capacity, Allocator *allocator) {
    if (!allocator || !allocator->alloc || item_size == 0) return NULL;
    if (capacity == 0) capacity = VECTOR_INITIAL_CAPACITY;

    if (capacity > (SIZE_MAX - sizeof(Vector_Header)) / item_size) return NULL;
    size_t total_size = sizeof(Vector_Header) + (item_size * capacity);

    Vector_Header *h = (Vector_Header *)allocator->alloc(NULL, total_size, allocator->context);
    if (!h) return NULL;

    h->capacity = capacity;
    h->length = 0;
    h->item_size = item_size;
    h->allocator = allocator;

    return (void *)(h + 1);
}

void *vector_ensure_capacity_impl(void *arr) {
    if (!arr) return NULL;

    Vector_Header *h = vector_hdr_(arr);
    if (h->length < h->capacity) {
        return arr; // Abundant capacity remaining
    }

    // Defensive overflow checks before capacity doubling
    if (h->capacity > SIZE_MAX / 2) return NULL;
    size_t new_capacity = h->capacity * 2;
    
    // Check for overflow in total size calculation
    if (h->item_size > 0 && new_capacity > (SIZE_MAX - sizeof(Vector_Header)) / h->item_size) return NULL;
    size_t new_total_size = sizeof(Vector_Header) + (h->item_size * new_capacity);

    // OOM-Safe Reallocation Pattern: preserves original buffer if allocator runs dry
    Vector_Header *new_h = (Vector_Header *)h->allocator->alloc(h, new_total_size, h->allocator->context);
    if (!new_h) return NULL; 

    new_h->capacity = new_capacity;
    
    return (void *)(new_h + 1);
}

void vector_free_impl(void *arr) {
    if (!arr) return;
    
    Vector_Header *h = vector_hdr_(arr);
    h->allocator->alloc(h, 0, h->allocator->context);
}
