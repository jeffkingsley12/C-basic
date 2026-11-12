#include <stdio.h>
#include <stdlib.h>
#include "vector.h"
#include "string_helper.h"

// Platform-agnostic standard runtime memory allocation bridge
void* std_allocator_bridge(void *ptr, size_t size, void *context) {
    (void)context;
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size);
}

int main(void) {
    // Initialize our allocation subsystem configurations
    Allocator runtime_alloc = { .alloc = std_allocator_bridge, .context = NULL };

    // Instantiate our type-safe vector to hold character pointers ('char*')
    char **string_vec = vector(char*, &runtime_alloc);
    if (!string_vec) {
        fprintf(stderr, "Vector initialization failed\n");
        return 1;
    }

    /* --- Memory Allocation & Dynamic Pushing --- */
    char *s1 = create_padded_string_impl("Hello", 8, &runtime_alloc);
    char *s2 = create_padded_string_impl(" the vector's internal O(1) array lookup mathematics are fully preserved.", 8, &runtime_alloc);

    if (!s1 || !s2) {
        fprintf(stderr, "Allocation failed\n");
        if (s1) runtime_alloc.alloc(s1, 0, runtime_alloc.context);
        if (s2) runtime_alloc.alloc(s2, 0, runtime_alloc.context);
        vector_free(string_vec);
        return 1;
    }

    int ok1 = 0, ok2 = 0;
#if defined(__GNUC__) || defined(__clang__)
    ok1 = vector_push(string_vec, s1);
    ok2 = vector_push(string_vec, s2);
#else
    vector_push(string_vec, s1, &ok1);
    vector_push(string_vec, s2, &ok2);
#endif

    if (!ok1 || !ok2) {
        fprintf(stderr, "Vector push failed\n");
        if (!ok1) runtime_alloc.alloc(s1, 0, runtime_alloc.context);
        if (!ok2) runtime_alloc.alloc(s2, 0, runtime_alloc.context);
        for (size_t i = 0; i < vector_length(string_vec); i++) {
            if (string_vec[i]) runtime_alloc.alloc(string_vec[i], 0, runtime_alloc.context);
        }
        vector_free(string_vec);
        return 1;
    }

    /* --- Processing Data Stream --- */
    printf("Vector Capacity: %zu | Current Elements Length: %zu\n", 
            vector_capacity(string_vec), vector_length(string_vec));
            
    for (size_t i = 0; i < vector_length(string_vec); i++) {
        // Safe, native array subscription access!
        printf("  String Element [%zu]: %s\n", i, string_vec[i]);
    }

    /* --- Clean Teardown Protocol --- */
    for (size_t i = 0; i < vector_length(string_vec); i++) {
        if (string_vec[i] != NULL) {
            runtime_alloc.alloc(string_vec[i], 0, runtime_alloc.context);
        }
    }

    vector_free(string_vec);
    printf("\nAll padded elements and vector metadata layers released cleanly.\n");
    return 0;
}