#include "arena.h"

#include <string.h>
#include <stdint.h>

static size_t arena_align_up(size_t value, size_t alignment) {
    if (alignment == 0) {
        return value;
    }
    size_t mask = alignment - 1u;
    return (value + mask) & ~mask;
}

static void *arena_find_payload(const Arena *arena, const void *ptr) {
    if (!arena || !ptr || !arena->buffer) {
        return NULL;
    }

    const uint8_t *buf = arena->buffer;
    const uint8_t *p = (const uint8_t *)ptr;
    if (p < buf || p >= buf + arena->capacity) {
        return NULL;
    }

    for (size_t cursor = 0; cursor < arena->offset;) {
        ArenaHeader *hdr = (ArenaHeader *)(buf + cursor);
        size_t payload_offset = arena_align_up(cursor + sizeof(ArenaHeader), arena->alignment ? arena->alignment : sizeof(void *));
        if (payload_offset + hdr->size > arena->capacity) {
            return NULL;
        }
        if (p == buf + payload_offset) {
            return (void *)p;
        }
        cursor = payload_offset + hdr->size;
    }

    return NULL;
}

void arena_init(Arena *arena, void *backing_buffer, size_t capacity, size_t alignment, int zero_on_alloc) {
    if (!arena) {
        return;
    }

    if (alignment == 0) {
        alignment = sizeof(void *);
    }
    if ((alignment & (alignment - 1u)) != 0u) {
        alignment = 1u;
        while (alignment < sizeof(void *)) {
            alignment <<= 1u;
        }
    }

    arena->buffer = (uint8_t *)backing_buffer;
    arena->capacity = capacity;
    arena->offset = 0;
    arena->alignment = alignment;
    arena->last_offset = 0;
    arena->last_size = 0;
    arena->zero_on_alloc = zero_on_alloc;
    arena->generation += 1u;
    memset(&arena->stats, 0, sizeof(arena->stats));
    if (arena->buffer && arena->capacity > 0) {
        memset(arena->buffer, 0, arena->capacity);
    }
}

void arena_reset(Arena *arena) {
    if (!arena) {
        return;
    }

    if (arena->buffer && arena->capacity > 0) {
        memset(arena->buffer, 0, arena->capacity);
    }
    arena->offset = 0;
    arena->last_offset = 0;
    arena->last_size = 0;
    arena->generation += 1u;
    memset(&arena->stats, 0, sizeof(arena->stats));
}

void *arena_alloc_impl(void *ptr, size_t size, void *context) {
    Arena *arena = (Arena *)context;
    if (!arena || !arena->buffer || arena->capacity == 0) {
        return NULL;
    }

    if (ptr != NULL && size == 0) {
        if (arena_find_payload(arena, ptr) == NULL) {
            return NULL;
        }

        for (size_t cursor = 0; cursor < arena->offset;) {
            ArenaHeader *hdr = (ArenaHeader *)(arena->buffer + cursor);
            size_t payload_offset = arena_align_up(cursor + sizeof(ArenaHeader), arena->alignment ? arena->alignment : sizeof(void *));
            if (arena->buffer + payload_offset == ptr) {
                arena->offset = cursor;
                arena->last_offset = 0;
                arena->last_size = 0;
                return NULL;
            }
            cursor = payload_offset + hdr->size;
        }
        return NULL;
    }

    if (ptr == NULL) {
        size_t payload_offset = arena_align_up(arena->offset, arena->alignment ? arena->alignment : sizeof(void *));
        size_t header_offset = arena_align_up(payload_offset + sizeof(ArenaHeader), arena->alignment ? arena->alignment : sizeof(void *));
        if (header_offset + size > arena->capacity) {
            return NULL;
        }

        ArenaHeader *hdr = (ArenaHeader *)(arena->buffer + payload_offset);
        hdr->magic = ARENA_HEADER_MAGIC;
        hdr->generation = arena->generation;
        hdr->size = size;
        hdr->owner = arena;

        if (arena->zero_on_alloc) {
            memset(arena->buffer + header_offset, 0, size);
        }

        arena->stats.lifetime_allocations += 1u;
        arena->stats.current_usage += size;
        arena->stats.current_peak_usage = arena->stats.current_usage > arena->stats.current_peak_usage ? arena->stats.current_usage : arena->stats.current_peak_usage;
        arena->stats.lifetime_peak_usage = arena->stats.current_peak_usage > arena->stats.lifetime_peak_usage ? arena->stats.current_peak_usage : arena->stats.lifetime_peak_usage;

        arena->last_offset = header_offset;
        arena->last_size = size;
        arena->offset = header_offset + size;
        return arena->buffer + header_offset;
    }

    /* Realloc-like behavior: resize the existing allocation in-place if possible. */
    for (size_t cursor = 0; cursor < arena->offset;) {
        ArenaHeader *hdr = (ArenaHeader *)(arena->buffer + cursor);
        size_t payload_offset = arena_align_up(cursor + sizeof(ArenaHeader), arena->alignment ? arena->alignment : sizeof(void *));
        if (arena->buffer + payload_offset == ptr) {
            size_t new_end = payload_offset + size;
            if (new_end > arena->capacity) {
                return NULL;
            }
            hdr->size = size;
            if (arena->zero_on_alloc) {
                memset(arena->buffer + payload_offset, 0, size);
            }
            arena->last_offset = payload_offset;
            arena->last_size = size;
            return arena->buffer + payload_offset;
        }
        cursor = payload_offset + hdr->size;
    }

    return NULL;
}

Allocator arena_as_allocator(Arena *arena) {
    Allocator allocator = {0};
    allocator.alloc = arena_alloc_impl;
    allocator.context = arena;
    return allocator;
}

bool arena_validate(const Arena *arena) {
    if (!arena || !arena->buffer || arena->capacity == 0) {
        return arena == NULL ? false : true;
    }

    size_t cursor = 0;
    while (cursor < arena->offset) {
        if (cursor + sizeof(ArenaHeader) > arena->capacity) {
            return false;
        }

        const ArenaHeader *hdr = (const ArenaHeader *)(arena->buffer + cursor);
        if (hdr->magic != ARENA_HEADER_MAGIC) {
            return false;
        }
        if (hdr->owner != arena) {
            return false;
        }
        if (hdr->generation != arena->generation) {
            return false;
        }

        size_t payload_offset = arena_align_up(cursor + sizeof(ArenaHeader), arena->alignment ? arena->alignment : sizeof(void *));
        if (payload_offset > arena->capacity) {
            return false;
        }
        if (hdr->size > arena->capacity - payload_offset) {
            return false;
        }

        cursor = payload_offset + hdr->size;
    }

    return cursor == arena->offset;
}
