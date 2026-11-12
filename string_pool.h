#ifndef STRING_POOL_H
#define STRING_POOL_H

#include "dict.h"
#include "string_lib.h"

/**
 * @file string_pool.h
 * @brief String Interning Pool for Compiler Frontends
 *
 * This module provides string deduplication for compiler tokenization and symbol tables.
 * Interned strings can be compared by pointer equality (O(1)) instead of string comparison (O(M)).
 *
 * This implementation uses the atomic String* type to leverage:
 * - O(1) cached hashing via string_hash()
 * - Thread-safe atomic reference counting
 * - Explicit length tracking
 *
 * THREAD SAFETY WARNING:
 * This implementation is NOT thread-safe. The underlying dictionary operations (set, get, resize)
 * are not protected by locks. For multi-threaded parsing:
 * - Option 1: Use thread-local pools (recommended) - give each thread its own StringPool
 * - Option 2: Add external synchronization (pthread_rwlock_t) around string_pool_intern calls
 */

typedef struct {
    Dictionary *map;       // Uses String* as keys and values
    Allocator *allocator;
} StringPool;

// Global or context-instantiated lifecycle management
StringPool* string_pool_create(Allocator *allocator);

/**
 * @brief Interns a String* into the pool.
 * @note OWNERSHIP CONTRACT:
 *   - On success (non-NULL return): consumes/adopts the caller's reference to `str` and returns
 *     a new owned reference to the canonical interned String*.
 *   - On failure (NULL return): does NOT consume `str`; the caller remains responsible for `string_unref(str)`.
 */
String* string_pool_intern(StringPool *pool, String *str);

void string_pool_free(StringPool *pool);

#endif // STRING_POOL_H
