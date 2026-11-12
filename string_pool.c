#include "string_pool.h"

StringPool* string_pool_create(Allocator *allocator) {
    if (!allocator || !allocator->alloc) return NULL;

    StringPool *pool = (StringPool*)allocator->alloc(NULL, sizeof(StringPool), allocator->context);
    if (!pool) return NULL;

    pool->allocator = allocator;
    pool->map = dict_create(256, allocator, NULL, 0);
    if (!pool->map) {
        allocator->alloc(pool, 0, allocator->context);
        return NULL;
    }
    return pool;
}

String* string_pool_intern(StringPool *pool, String *str) {
    if (!pool || !str) return NULL;

    String *existing = (String*)dict_get_string_key(pool->map, str);
    if (existing) {
        /* Increment existing before unreffing str in case existing == str */
        string_ref(existing);
        string_unref(str);
        return existing;
    }

    string_ref(str);
    if (!dict_set_string_key(pool->map, str, str)) {
        string_unref(str);
        return NULL;
    }
    return str;
}

void string_pool_free(StringPool *pool) {
    if (!pool) return;

    size_t idx = 0;
    const Dict_Entry *entry;
    while ((entry = dict_iter(pool->map, &idx))) {
        if (entry->key) {
            string_unref((String*)entry->key);
        }
    }

    dict_free(pool->map);
    pool->allocator->alloc(pool, 0, pool->allocator->context);
}
