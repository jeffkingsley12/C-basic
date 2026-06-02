#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef void* (*AllocatorFunc)(void *ptr, size_t size, void *context);

typedef struct {
    AllocatorFunc alloc;
    void *context;
} Allocator;

struct Arena;

typedef struct {
    uint64_t magic;
    uint64_t generation;
    size_t size;
    struct Arena *owner;   /* kept for debugging; primary check is generation */
} ArenaHeader;

#define ARENA_HEADER_MAGIC 0x4152454E41484452ULL

typedef struct {
    size_t lifetime_allocations;
    size_t lifetime_reallocations;
    size_t current_usage;
    size_t current_peak_usage;
    size_t lifetime_peak_usage;
} ArenaStats;

typedef struct Arena {
    uint8_t *buffer;
    size_t capacity;
    size_t offset;
    size_t alignment;
    size_t last_offset;
    size_t last_size;
    int zero_on_alloc;
    ArenaStats stats;
    uint64_t generation;   /* monotonic ID, incremented per init */
#ifdef DEBUG
    size_t allocation_count;   /* blocks currently live */
#endif
} Arena;

void      arena_init(Arena *arena, void *backing_buffer, size_t capacity, size_t alignment, int zero_on_alloc);
void      arena_reset(Arena *arena);
void     *arena_alloc_impl(void *ptr, size_t size, void *context);
Allocator arena_as_allocator(Arena *arena);
bool      arena_validate(const Arena *arena);

#endif
