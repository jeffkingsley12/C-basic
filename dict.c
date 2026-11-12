#include "dict.h"
#include "string_helper.h"
#include "string_lib.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define TOMBSTONE -2
#define EMPTY     -1
#define DICT_MAX_CAPACITY (1ULL << 30)
#define NEEDS_RESIZE(used, cap) (((used) + 1) * 3 > (cap) * 2)

typedef enum {
    KEY_MODE_CSTR,
    KEY_MODE_INTERNED_PTR,
    KEY_MODE_STRING_OBJ
} DictKeyMode;

static uint32_t hash_string(const char *key) {
    uint32_t hash = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)key; *p; p++) {
        hash ^= *p;
        hash *= 16777619u;
    }
    return hash;
}

static inline int dict_entries_push(Dict_Entry **entries, Dict_Entry entry) {
    int ok = 0;
#if defined(__GNUC__) || defined(__clang__)
    ok = vector_push(*entries, entry);
#else
    vector_push(*entries, entry, &ok);
#endif
    return ok;
}

static bool keys_match(const char *entry_key, uint32_t entry_hash,
                       const void *query_key, uint32_t query_hash, DictKeyMode mode) {
    if (!entry_key) return false;
    switch (mode) {
        case KEY_MODE_INTERNED_PTR:
            return (const void *)entry_key == query_key;
        case KEY_MODE_STRING_OBJ:
            return entry_hash == query_hash && string_equal_void(entry_key, query_key);
        case KEY_MODE_CSTR:
        default:
            return entry_hash == query_hash && strcmp(entry_key, (const char *)query_key) == 0;
    }
}

static int dict_compact(Dictionary *d);

Dictionary* dict_create(size_t initial_capacity, Allocator *allocator, DictValueDestructor value_destructor, int owns_keys) {
    if (!allocator || !allocator->alloc) return NULL;
    if (initial_capacity > DICT_MAX_CAPACITY) return NULL;

    size_t capacity = 8;
    while (capacity < initial_capacity) capacity <<= 1;

    Dictionary *d = (Dictionary*)allocator->alloc(NULL, sizeof(Dictionary), allocator->context);
    if (!d) return NULL;

    d->allocator = allocator;
    d->value_destructor = value_destructor;
    d->owns_keys = owns_keys;
    d->mask = capacity - 1;
    d->used_slots = 0;
    d->live_count = 0;

    d->indices = (int32_t*)allocator->alloc(NULL, capacity * sizeof(int32_t), allocator->context);
    if (!d->indices) {
        allocator->alloc(d, 0, allocator->context);
        return NULL;
    }
    memset(d->indices, 0xFF, capacity * sizeof(int32_t));

    d->entries = vector(Dict_Entry, allocator);
    if (!d->entries) {
        allocator->alloc(d->indices, 0, allocator->context);
        allocator->alloc(d, 0, allocator->context);
        return NULL;
    }
    return d;
}

static int dict_resize(Dictionary *d) {
    size_t old_cap = d->mask + 1;
    if (old_cap >= DICT_MAX_CAPACITY) return 0;
    size_t new_cap = old_cap * 2;

    int32_t *new_idx = (int32_t*)d->allocator->alloc(
        NULL, new_cap * sizeof(int32_t), d->allocator->context);
    if (!new_idx) return 0;

    memset(new_idx, 0xFF, new_cap * sizeof(int32_t));
    size_t new_mask = new_cap - 1;
    size_t len = vector_length(d->entries);
    size_t write = 0;

    /* Compact dead entries in-place while re-indexing into the larger table */
    for (size_t read = 0; read < len; read++) {
        if (d->entries[read].key == NULL) continue;
        if (write != read) {
            d->entries[write] = d->entries[read];
        }
        size_t idx = d->entries[write].hash & new_mask;
        while (new_idx[idx] != EMPTY) {
            idx = (idx + 1) & new_mask;
        }
        new_idx[idx] = (int32_t)write;
        write++;
    }

    if (write != len) {
        vector_truncate(d->entries, write);
    }

    d->allocator->alloc(d->indices, 0, d->allocator->context);
    d->indices = new_idx;
    d->mask = new_mask;
    d->used_slots = write;
    d->live_count = write;
    return 1;
}

static int dict_set_internal(Dictionary *d, const void *key, uint32_t hash, void *value, DictKeyMode mode) {
    if (!d || !key) return 0;

    if (NEEDS_RESIZE(d->used_slots, d->mask + 1)) {
        if (!dict_resize(d)) return 0;
    }

    size_t idx = hash & d->mask;
    int32_t first_tomb = -1;

    while (d->indices[idx] != EMPTY) {
        int32_t entry_i = d->indices[idx];
        if (entry_i == TOMBSTONE) {
            if (first_tomb == -1) first_tomb = (int32_t)idx;
        } else {
            Dict_Entry *e = &d->entries[entry_i];
            if (keys_match(e->key, e->hash, key, hash, mode)) {
                if (d->value_destructor && e->value && e->value != value) {
                    d->value_destructor(e->value, d->allocator);
                }
                e->value = value;
                return 1;
            }
        }
        idx = (idx + 1) & d->mask;
    }

    if (vector_length(d->entries) >= (size_t)INT32_MAX) return 0;

    char *stored_key = (char *)key;
    if (mode == KEY_MODE_CSTR && d->owns_keys) {
        stored_key = create_padded_string_impl((const char *)key, 8, d->allocator);
        if (!stored_key) return 0;
    }

    Dict_Entry new_entry = { stored_key, value, hash };
    int32_t entry_idx = (int32_t)vector_length(d->entries);

    if (!dict_entries_push(&d->entries, new_entry)) {
        if (mode == KEY_MODE_CSTR && d->owns_keys) {
            d->allocator->alloc(stored_key, 0, d->allocator->context);
        }
        return 0;
    }

    d->indices[(first_tomb != -1) ? (size_t)first_tomb : idx] = entry_idx;
    if (first_tomb == -1) d->used_slots++;
    d->live_count++;
    return 1;
}

int dict_set(Dictionary *d, const char *key, void *value) {
    if (!d || !key) return 0;
    return dict_set_internal(d, key, hash_string(key), value, KEY_MODE_CSTR);
}

int dict_set_interned(Dictionary *d, const char *interned_key, void *value) {
    if (!d || !interned_key) return 0;
    return dict_set_internal(d, interned_key, hash_string(interned_key), value, KEY_MODE_INTERNED_PTR);
}

int dict_set_string_key(Dictionary *d, const void *string_key, void *value) {
    if (!d || !string_key) return 0;
    return dict_set_internal(d, string_key, string_hash_void(string_key), value, KEY_MODE_STRING_OBJ);
}

static const Dict_Entry* dict_find_entry(const Dictionary *d, const void *key, uint32_t hash, DictKeyMode mode) {
    if (!d || !key) return NULL;
    size_t idx = hash & d->mask;
    while (d->indices[idx] != EMPTY) {
        int32_t entry_i = d->indices[idx];
        if (entry_i != TOMBSTONE) {
            const Dict_Entry *e = &d->entries[entry_i];
            if (keys_match(e->key, e->hash, key, hash, mode)) {
                return e;
            }
        }
        idx = (idx + 1) & d->mask;
    }
    return NULL;
}

void* dict_get(const Dictionary *d, const char *key) {
    if (!d || !key) return NULL;
    const Dict_Entry *e = dict_find_entry(d, key, hash_string(key), KEY_MODE_CSTR);
    return e ? e->value : NULL;
}

const char* dict_get_key_pointer(const Dictionary *d, const char *key) {
    if (!d || !key) return NULL;
    const Dict_Entry *e = dict_find_entry(d, key, hash_string(key), KEY_MODE_CSTR);
    return e ? e->key : NULL;
}

void* dict_get_interned(const Dictionary *d, const char *interned_key) {
    if (!d || !interned_key) return NULL;
    const Dict_Entry *e = dict_find_entry(d, interned_key, hash_string(interned_key), KEY_MODE_INTERNED_PTR);
    return e ? e->value : NULL;
}

void* dict_get_string_key(const Dictionary *d, const void *string_key) {
    if (!d || !string_key) return NULL;
    const Dict_Entry *e = dict_find_entry(d, string_key, string_hash_void(string_key), KEY_MODE_STRING_OBJ);
    return e ? e->value : NULL;
}

void* dict_get_interned_string(const Dictionary *d, const void *interned_key) {
    if (!d || !interned_key) return NULL;
    const Dict_Entry *e = dict_find_entry(d, interned_key, string_hash_void(interned_key), KEY_MODE_INTERNED_PTR);
    return e ? e->value : NULL;
}

int dict_contains(const Dictionary *d, const char *key) {
    if (!d || !key) return 0;
    return dict_find_entry(d, key, hash_string(key), KEY_MODE_CSTR) != NULL;
}

static int dict_del_internal(Dictionary *d, const void *key, uint32_t hash, DictKeyMode mode) {
    if (!d || !key) return 0;
    size_t idx = hash & d->mask;
    while (d->indices[idx] != EMPTY) {
        if (d->indices[idx] != TOMBSTONE) {
            Dict_Entry *e = &d->entries[d->indices[idx]];
            if (keys_match(e->key, e->hash, key, hash, mode)) {
                if (mode == KEY_MODE_CSTR && d->owns_keys) {
                    d->allocator->alloc(e->key, 0, d->allocator->context);
                }
                if (d->value_destructor && e->value) {
                    d->value_destructor(e->value, d->allocator);
                }
                e->key = NULL;
                e->value = NULL;
                d->indices[idx] = TOMBSTONE;
                d->live_count--;
                if (vector_length(d->entries) > d->live_count * 2
                    && vector_length(d->entries) > 64) {
                    dict_compact(d);
                }
                return 1;
            }
        }
        idx = (idx + 1) & d->mask;
    }
    return 0;
}

int dict_del(Dictionary *d, const char *key) {
    if (!d || !key) return 0;
    return dict_del_internal(d, key, hash_string(key), KEY_MODE_CSTR);
}

int dict_del_interned(Dictionary *d, const char *interned_key) {
    if (!d || !interned_key) return 0;
    return dict_del_internal(d, interned_key, hash_string(interned_key), KEY_MODE_INTERNED_PTR);
}

int dict_del_string_key(Dictionary *d, const void *string_key) {
    if (!d || !string_key) return 0;
    return dict_del_internal(d, string_key, string_hash_void(string_key), KEY_MODE_STRING_OBJ);
}

int dict_del_interned_string(Dictionary *d, const void *interned_key) {
    if (!d || !interned_key) return 0;
    return dict_del_internal(d, interned_key, string_hash_void(interned_key), KEY_MODE_INTERNED_PTR);
}

size_t dict_length(const Dictionary *d) {
    return d ? d->live_count : 0;
}

const Dict_Entry* dict_iter(const Dictionary *d, size_t *index) {
    if (!d || !index) return NULL;
    size_t len = vector_length(d->entries);
    while (*index < len) {
        const Dict_Entry *e = &d->entries[(*index)++];
        if (e->key) return e;
    }
    return NULL;
}

static int dict_compact(Dictionary *d) {
    size_t len = vector_length(d->entries);
    if (len == 0) return 1;

    size_t write = 0;
    for (size_t read = 0; read < len; read++) {
        if (d->entries[read].key) {
            if (read != write) {
                d->entries[write] = d->entries[read];
            }
            write++;
        }
    }

    if (write == len && d->used_slots == write) {
        return 1;
    }

    vector_truncate(d->entries, write);

    size_t cap = d->mask + 1;
    int32_t *new_idx = (int32_t*)d->allocator->alloc(
        NULL, cap * sizeof(int32_t), d->allocator->context);
    if (!new_idx) return 0;

    memset(new_idx, 0xFF, cap * sizeof(int32_t));

    for (size_t i = 0; i < write; i++) {
        uint32_t hash = d->entries[i].hash;
        size_t idx = hash & d->mask;
        while (new_idx[idx] != EMPTY) {
            idx = (idx + 1) & d->mask;
        }
        new_idx[idx] = (int32_t)i;
    }

    d->allocator->alloc(d->indices, 0, d->allocator->context);
    d->indices = new_idx;
    d->used_slots = write;
    d->live_count = write;
    return 1;
}

void dict_free(Dictionary *d) {
    if (!d) return;
    size_t len = vector_length(d->entries);
    for (size_t i = 0; i < len; i++) {
        if (d->entries[i].key) {
            if (d->owns_keys) {
                d->allocator->alloc(d->entries[i].key, 0, d->allocator->context);
            }
            if (d->value_destructor && d->entries[i].value) {
                d->value_destructor(d->entries[i].value, d->allocator);
            }
        }
    }
    vector_free(d->entries);
    d->allocator->alloc(d->indices, 0, d->allocator->context);
    d->allocator->alloc(d, 0, d->allocator->context);
}

void dict_reverse(Dictionary *d) {
    if (!d || d->live_count <= 1) return;
    if (!dict_compact(d)) return;

    size_t len = vector_length(d->entries);
    for (size_t i = 0; i < len / 2; i++) {
        Dict_Entry temp = d->entries[i];
        d->entries[i] = d->entries[len - 1 - i];
        d->entries[len - 1 - i] = temp;
    }

    size_t capacity = d->mask + 1;
    for (size_t i = 0; i < capacity; i++) {
        int32_t idx = d->indices[i];
        if (idx != EMPTY && idx != TOMBSTONE) {
            d->indices[i] = (int32_t)(len - 1 - (size_t)idx);
        }
    }
}

const Dict_Entry* dict_min(const Dictionary *d, DictComparator comp) {
    if (!d || d->live_count == 0 || !comp) return NULL;
    const Dict_Entry *min_entry = NULL;
    size_t len = vector_length(d->entries);

    for (size_t i = 0; i < len; i++) {
        if (d->entries[i].key == NULL) continue;
        if (!min_entry || comp(&d->entries[i], min_entry) < 0) {
            min_entry = &d->entries[i];
        }
    }
    return min_entry;
}

const Dict_Entry* dict_max(const Dictionary *d, DictComparator comp) {
    if (!d || d->live_count == 0 || !comp) return NULL;
    const Dict_Entry *max_entry = NULL;
    size_t len = vector_length(d->entries);

    for (size_t i = 0; i < len; i++) {
        if (d->entries[i].key == NULL) continue;
        if (!max_entry || comp(&d->entries[i], max_entry) > 0) {
            max_entry = &d->entries[i];
        }
    }
    return max_entry;
}

void dict_sort(Dictionary *d, DictComparator comp) {
    if (!d || d->live_count <= 1 || !comp) return;
    if (!dict_compact(d)) return;
    size_t len = vector_length(d->entries);

    qsort(d->entries, len, sizeof(Dict_Entry), (int(*)(const void*, const void*))comp);

    size_t capacity = d->mask + 1;
    memset(d->indices, 0xFF, capacity * sizeof(int32_t));
    d->used_slots = d->live_count;

    for (size_t i = 0; i < len; i++) {
        uint32_t hash = d->entries[i].hash;
        size_t idx = hash & d->mask;
        while (d->indices[idx] != EMPTY) {
            idx = (idx + 1) & d->mask;
        }
        d->indices[idx] = (int32_t)i;
    }
}
