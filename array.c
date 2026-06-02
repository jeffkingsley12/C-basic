#include "array.h"
#include <string.h> // For memcpy

void *array_init(size_t item_size, size_t capacity, Allocator *a) {
    if (!a || !a->alloc) return NULL;

    void *ptr = NULL;
    size_t total_size = sizeof(Array_Header) + (item_size * capacity);
    
    // Allocate memory block: size bytes, passing custom context
    Array_Header *h = (Array_Header *)a->alloc(total_size, a->context);
    
    if (h) {
        h->capacity = capacity;
        h->length = 0;
        h->a = a;
        ptr = (void *)(h + 1); // Move pointer right past header to data region
    }
    
    return ptr;
}

void array_free_impl(void *arr) {
    if (!arr) return;
    
    Array_Header *h = (Array_Header *)arr - 1;
    Allocator *a = h->a;
    
    if (a && a->alloc) {
        // Passing 0 size to indicate deallocation in custom interfaces
        a->alloc(0, h->context); 
    }
}

void *array_ensure_capacity(void *arr, size_t item_size) {
    if (!arr) return NULL;

    Array_Header *h = (Array_Header *)arr - 1;
    Allocator *a = h->a;

    if (h->length >= h->capacity) {
        size_t new_capacity = h->capacity == 0 ? ARRAY_INITIAL_CAPACITY : h->capacity * 2;
        size_t old_total_size = sizeof(Array_Header) + (h->capacity * item_size);
        size_t new_total_size = sizeof(Array_Header) + (new_capacity * item_size);

        // Custom Allocators typically require either a dedicated realloc token 
        // or a fresh allocation paired with manual migration. 
        // We create a fresh block to remain universally compatible with basic arenas/pools.
        Array_Header *new_h = (Array_Header *)a->alloc(new_total_size, a->context);
        if (!new_h) {
            return arr; // Fail safe: Return old array block untouched to prevent leaks
        }

        // Migrate header meta and payload elements
        new_h->capacity = new_capacity;
        new_h->length = h->length;
        new_h->a = a;
        
        memcpy(new_h + 1, h + 1, h->length * item_size);

        // Free old tracking block
        a->alloc(0, h->context);

        return (void *)(new_h + 1);
    }

    return arr;
}
