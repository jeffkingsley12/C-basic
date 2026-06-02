#include "arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ARENA_CAP (1024 * 1024)
#define MAX_ALLOCS 256
#define ITERATIONS 1000000

typedef struct {
    void *ptr;
    size_t size;
    int live;
} Slot;

static uint8_t backing[ARENA_CAP];
static Slot slots[MAX_ALLOCS];
static Arena arena;

static void inject_corruption(void) {
    int target = rand() % 4;
    switch (target) {
        case 0:
            /* Corrupt a random header's size */
            if (arena.offset > sizeof(ArenaHeader)) {
                size_t off = (rand() % (arena.offset - sizeof(ArenaHeader)));
                ArenaHeader *h = (ArenaHeader *)(arena.buffer + off);
                h->size = (size_t)rand() << 32 | rand();
            }
            break;
        case 1:
            /* Corrupt magic */
            if (arena.offset > sizeof(ArenaHeader)) {
                size_t off = (rand() % (arena.offset - sizeof(ArenaHeader)));
                ArenaHeader *h = (ArenaHeader *)(arena.buffer + off);
                h->magic ^= 0xDEADBEEF;
            }
            break;
        case 2:
            /* Corrupt offset */
            arena.offset = (size_t)rand() << 32 | rand();
            break;
        case 3:
            /* Corrupt last_offset */
            arena.last_offset = (size_t)rand() << 32 | rand();
            break;
    }
}

int main(void) {
    srand((unsigned)time(NULL));
    arena_init(&arena, backing, ARENA_CAP, 16, 0);

    size_t allocs = 0, reallocs = 0, frees = 0, resets = 0, validations = 0, corruptions = 0;

    for (size_t i = 0; i < ITERATIONS; i++) {
        int op = rand() % 100;

        if (op < 40) {
            /* Random alloc */
            size_t sz = rand() % 4096 + 1;
            int idx = rand() % MAX_ALLOCS;
            if (!slots[idx].live) {
                slots[idx].ptr = arena_alloc_impl(NULL, sz, &arena);
                if (slots[idx].ptr) {
                    slots[idx].size = sz;
                    slots[idx].live = 1;
                    allocs++;
                }
            }
        } else if (op < 70) {
            /* Random realloc (grow or shrink) */
            int idx = rand() % MAX_ALLOCS;
            if (slots[idx].live) {
                size_t sz = rand() % 8192 + 1;
                void *np = arena_alloc_impl(slots[idx].ptr, sz, &arena);
                if (np) {
                    slots[idx].ptr = np;
                    slots[idx].size = sz;
                    reallocs++;
                }
            }
        } else if (op < 80) {
            /* Realloc-to-zero (diagnostic path) */
            int idx = rand() % MAX_ALLOCS;
            if (slots[idx].live) {
                arena_alloc_impl(slots[idx].ptr, 0, &arena);
                /* NOTE: does NOT free metadata; pointer stays live in our book-keeping */
                frees++;
            }
        } else if (op < 90) {
            /* Validation sweep */
            bool ok = arena_validate(&arena);
            validations++;
            if (!ok) {
                /* Corruption detected — expected after injection */
                arena_reset(&arena);
                memset(slots, 0, sizeof(slots));
                resets++;
            }
        } else if (op < 95) {
            /* Reset */
            arena_reset(&arena);
            memset(slots, 0, sizeof(slots));
            resets++;
        } else {
            /* Corruption injection */
            inject_corruption();
            corruptions++;
        }
    }

    printf("Iterations: %zu\n", ITERATIONS);
    printf("Allocs:     %zu\n", allocs);
    printf("Reallocs:   %zu\n", reallocs);
    printf("Zero-reallocs: %zu\n", frees);
    printf("Validations: %zu\n", validations);
    printf("Resets:     %zu\n", resets);
    printf("Corruptions injected: %zu\n", corruptions);
    printf("Final validate: %s\n", arena_validate(&arena) ? "PASS" : "FAIL");

    return 0;
}
