#include "array.h"
#include <string.h> 

void *array_init(size_t item_size, size_t capacity, Allocator *allocator) {
    if (!allocator || !allocator->alloc) return NULL;

    size_t total_size = sizeof(Array_Header) + (item_size * capacity);
    
    // Pass NULL as ptr to indicate fresh allocation (malloc behavior)
    Array_Header *h = (Array_Header *)allocator->alloc(NULL, total_size, allocator->context);
    
    if (h) {
        h->capacity = capacity;
        h->length = 0;
        h->item_size = item_size;
        h->allocator = allocator;
        return (void *)(h + 1); 
    }
    
    return NULL;
}

void array_free_impl(void *arr) {
    if (!arr) return;
    
    Array_Header *h = (Array_Header *)arr - 1;
    Allocator *a = h->allocator;
    
    if (a && a->alloc) {
        // Pass pointer and size 0 to indicate deallocation (free behavior)
        a->alloc(h, 0, a->context); 
    }
}

void *array_ensure_capacity(void *arr) {
    if (!arr) return NULL; 

    Array_Header *h = (Array_Header *)arr - 1;
    Allocator *a = h->allocator;

    if (h->length >= h->capacity) {
        size_t new_capacity = h->capacity == 0 ? ARRAY_INITIAL_CAPACITY : h->capacity * 2;
        size_t new_total_size = sizeof(Array_Header) + (new_capacity * h->item_size);

        // IN-PLACE RESIZING: Pass 'h' to allow standard realloc to expand in-place.
        // If the allocator relocates, it handles the memcpy automatically.
        Array_Header *new_h = (Array_Header *)a->alloc(h, new_total_size, a->context);
        
        // OOM SAFETY: If allocation fails, new_h is NULL. 
        // The allocator contract guarantees 'h' is still valid.
        if (!new_h) return NULL; 

        // Update capacity (length and item_size remain unchanged)
        new_h->capacity = new_capacity;
        return (void *)(new_h + 1);
    }

    return arr;
}
