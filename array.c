#include "array.h"
#include <string.h> 

void *array_init(size_t item_size, size_t capacity, Allocator *allocator) {
    if (!allocator || !allocator->alloc) return NULL;

    size_t total_size = sizeof(Array_Header) + (item_size * capacity);
    
    // Pass NULL as ptr to indicate fresh allocation
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
        // Pass pointer and size 0 to indicate deallocation
        a->alloc(h, 0, a->context); 
    }
}

// FIXED: Removed item_size parameter. Returns NULL on OOM to prevent macro corruption.
void *array_ensure_capacity(void *arr) {
    if (!arr) return NULL; 

    Array_Header *h = (Array_Header *)arr - 1;
    Allocator *a = h->allocator;

    if (h->length >= h->capacity) {
        size_t new_capacity = h->capacity == 0 ? ARRAY_INITIAL_CAPACITY : h->capacity * 2;
        
        // Use h->item_size instead of requiring it as a parameter
        size_t new_total_size = sizeof(Array_Header) + (new_capacity * h->item_size);

        // Allocate fresh block to remain universally compatible with basic arenas/pools
        Array_Header *new_h = (Array_Header *)a->alloc(NULL, new_total_size, a->context);
        
        // CRITICAL FIX: Return NULL on failure. 
        // If we returned the old pointer, the macro would assume success and write out of bounds.
        if (!new_h) {
            return NULL; 
        }

        new_h->capacity = new_capacity;
        new_h->length = h->length;
        new_h->item_size = h->item_size;
        new_h->allocator = h->allocator;
        
        memcpy(new_h + 1, h + 1, h->length * h->item_size);

        // Free old tracking block
        a->alloc(h, 0, a->context);

        return (void *)(new_h + 1);
    }

    return arr;
}
