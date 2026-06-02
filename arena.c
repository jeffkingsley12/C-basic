#include "arena.h"
#include <string.h>

#define MIN_SIZE(a, b) ((a) < (b) ? (a) : (b))

static uint64_t g_arena_seq = 0;

static inline bool align_up_safe(size_t value, size_t alignment, size_t *out) {
    if (value > SIZE_MAX - (alignment - 1)) return false;
    *out = (value + (alignment - 1)) & ~(alignment - 1);
    return true;
}

void arena_init(Arena *arena, void *backing_buffer, size_t capacity, size_t alignment, int zero_on_alloc) {
    if (!arena || !backing_buffer) return;
    if (alignment < 16 || alignment > 128 || (alignment & (alignment - 1)) != 0)
        alignment = 16;

    uintptr_t base_addr = (uintptr_t)backing_buffer;
    uintptr_t mask = ~(uintptr_t)(alignment - 1);
    uintptr_t aligned_base = (base_addr + (alignment - 1)) & mask;
    size_t padding = aligned_base - base_addr;

    if (padding >= capacity) {
        arena->buffer = NULL;
        arena->capacity = 0;
    } else {
        arena->buffer = (uint8_t *)aligned_base;
        arena->capacity = capacity - padding;
    }

    arena->offset = 0;
    arena->alignment = alignment;
    arena->zero_on_alloc = zero_on_alloc;
    arena->last_offset = SIZE_MAX;
    arena->last_size = 0;
    arena->generation = ++g_arena_seq;
    memset(&arena->stats, 0, sizeof(ArenaStats));
#ifdef DEBUG
    arena->allocation_count = 0;
#endif
}

void arena_reset(Arena *arena) {
    if (!arena) return;
#ifdef DEBUG
    if (arena->buffer && arena->capacity > 0)
        memset(arena->buffer, 0xDD, arena->capacity);
#endif
    arena->offset = 0;
    arena->last_offset = SIZE_MAX;
    arena->last_size = 0;
    arena->stats.current_usage = 0;
    arena->stats.current_peak_usage = 0;
#ifdef DEBUG
    arena->allocation_count = 0;
#endif
}

void *arena_alloc_impl(void *ptr, size_t size, void *context) {
    Arena *arena = (Arena *)context;
    if (!arena || !arena->buffer) return NULL;
    if (size == 0) return NULL;

    if (ptr == NULL) {
        size_t current_offset = arena->offset;
        if (current_offset > SIZE_MAX - sizeof(ArenaHeader)) return NULL;
        size_t raw_payload_offset = current_offset + sizeof(ArenaHeader);

        size_t payload_offset;
        if (!align_up_safe(raw_payload_offset, arena->alignment, &payload_offset)) return NULL;

        if (size > SIZE_MAX - payload_offset) return NULL;
        size_t payload_end = payload_offset + size;

        size_t next_offset;
        if (!align_up_safe(payload_end, arena->alignment, &next_offset)) return NULL;
        if (next_offset > arena->capacity) return NULL;

        /* CRITICAL: verify header_offset is within bounds BEFORE dereference */
        size_t header_offset = payload_offset - sizeof(ArenaHeader);
        if (header_offset > arena->offset) return NULL;   /* stronger invariant */

        ArenaHeader *header = (ArenaHeader *)(arena->buffer + header_offset);
        header->size = size;
        header->owner = arena;
        header->generation = arena->generation;
        header->magic = ARENA_HEADER_MAGIC;

        arena->last_offset = payload_offset;
        arena->last_size = size;
        arena->offset = next_offset;

        arena->stats.lifetime_allocations++;
        arena->stats.current_usage = next_offset;
        if (next_offset > arena->stats.current_peak_usage)
            arena->stats.current_peak_usage = next_offset;
        if (next_offset > arena->stats.lifetime_peak_usage)
            arena->stats.lifetime_peak_usage = next_offset;

#ifdef DEBUG
        arena->allocation_count++;
#endif

        void *final_ptr = (void *)(arena->buffer + payload_offset);
        if (arena->zero_on_alloc)
            memset(final_ptr, 0, size);
        return final_ptr;
    }

    /* Reallocation */
    uint8_t *old_payload_ptr = (uint8_t *)ptr;
    if (old_payload_ptr < arena->buffer || old_payload_ptr >= (arena->buffer + arena->capacity))
        return NULL;

    size_t old_offset = old_payload_ptr - arena->buffer;
    if (old_offset < sizeof(ArenaHeader)) return NULL;

    /* CRITICAL: verify header_offset is within bounds BEFORE dereference */
    size_t old_header_offset = old_offset - sizeof(ArenaHeader);
    if (old_header_offset > arena->offset) return NULL;

    ArenaHeader *old_header = (ArenaHeader *)(arena->buffer + old_header_offset);
    if (old_header->magic != ARENA_HEADER_MAGIC) return NULL;
    if (old_header->generation != arena->generation) return NULL;
    if (old_header->owner != arena) return NULL;

    size_t old_size = old_header->size;

    size_t expected_tail_offset;
    if (!align_up_safe(old_offset + old_size, arena->alignment, &expected_tail_offset)) return NULL;

    if (old_offset == arena->last_offset && old_size == arena->last_size &&
        arena->offset == expected_tail_offset) {

        if (size > SIZE_MAX - old_offset) return NULL;
        size_t new_payload_end = old_offset + size;
        size_t new_next_offset;
        if (!align_up_safe(new_payload_end, arena->alignment, &new_next_offset)) return NULL;
        if (new_next_offset > arena->capacity) return NULL;

        old_header->size = size;
        arena->last_size = size;
        arena->offset = new_next_offset;

        arena->stats.lifetime_reallocations++;
        arena->stats.current_usage = new_next_offset;
        if (new_next_offset > arena->stats.current_peak_usage)
            arena->stats.current_peak_usage = new_next_offset;
        if (new_next_offset > arena->stats.lifetime_peak_usage)
            arena->stats.lifetime_peak_usage = new_next_offset;

        if (arena->zero_on_alloc && size > old_size)
            memset(old_payload_ptr + old_size, 0, size - old_size);

        return ptr;
    }

    /* Fallback migration */
    void *new_payload_ptr = arena_alloc_impl(NULL, size, context);
    if (!new_payload_ptr) return NULL;

    size_t copy_bytes = MIN_SIZE(old_size, size);
    memcpy(new_payload_ptr, old_payload_ptr, copy_bytes);

    arena->stats.lifetime_reallocations++;
    return new_payload_ptr;
}

Allocator arena_as_allocator(Arena *arena) {
    Allocator alloc = { arena_alloc_impl, arena };
    return alloc;
}

bool arena_validate(const Arena *arena) {
    if (!arena || !arena->buffer) return false;

    size_t current = 0;
#ifdef DEBUG
    size_t visited_blocks = 0;
#endif

    while (current < arena->offset) {
        if (current > SIZE_MAX - sizeof(ArenaHeader)) return false;

        size_t raw = current + sizeof(ArenaHeader);
        size_t payload_offset;
        if (!align_up_safe(raw, arena->alignment, &payload_offset)) return false;

        /* STRONGER INVARIANT: header must land within committed bump space */
        if (payload_offset > arena->offset) return false;
        if (payload_offset < sizeof(ArenaHeader)) return false; /* sanity */

        size_t header_offset = payload_offset - sizeof(ArenaHeader);
        /* CRITICAL: bounds-check BEFORE dereference */
        if (header_offset > arena->offset) return false;

        ArenaHeader *hdr = (ArenaHeader *)(arena->buffer + header_offset);

        if (hdr->magic != ARENA_HEADER_MAGIC) return false;
        if (hdr->generation != arena->generation) return false;
        if (hdr->owner != arena) return false;

        /* Overflow protection on corrupted sizes */
        if (hdr->size > SIZE_MAX - payload_offset) return false;
        size_t payload_end = payload_offset + hdr->size;

        size_t next;
        if (!align_up_safe(payload_end, arena->alignment, &next)) return false;

        /* Forward progress and bounds */
        if (next <= current) return false;
        if (next > arena->offset) return false;

        current = next;
#ifdef DEBUG
        visited_blocks++;
#endif
    }

#ifdef DEBUG
    if (visited_blocks != arena->allocation_count) return false;
#endif

    return current == arena->offset;
}
