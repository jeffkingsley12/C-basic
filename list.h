#ifndef DICT_H
#define DICT_H

#include <stddef.h>
#include <stdint.h>
#include "ce_types.h"
#include "vector.h"

typedef struct {
    char *key;
    void *value;
    uint32_t hash;
} Dict_Entry;

typedef void (*DictValueDestructor)(void *value, Allocator *allocator);
typedef int (*DictComparator)(const Dict_Entry *a, const Dict_Entry *b);

typedef struct {
    int32_t *indices;
    Dict_Entry *entries;
    size_t mask;
    size_t used_slots;
    size_t live_count;
    Allocator *allocator;
    DictValueDestructor value_destructor;
    int owns_keys;
} Dictionary;

Dictionary *dict_create(size_t initial_capacity, Allocator *allocator, DictValueDestructor value_destructor, int owns_keys);
int dict_set(Dictionary *d, const char *key, void *value);
int dict_set_interned(Dictionary *d, const char *interned_key, void *value);
int dict_set_string_key(Dictionary *d, const void *string_key, void *value);
void *dict_get(const Dictionary *d, const char *key);
const char *dict_get_key_pointer(const Dictionary *d, const char *key);
void *dict_get_interned(const Dictionary *d, const char *interned_key);
void *dict_get_interned_string(const Dictionary *d, const void *interned_key);
void *dict_get_string_key(const Dictionary *d, const void *string_key);
int dict_del(Dictionary *d, const char *key);
int dict_del_interned(Dictionary *d, const char *interned_key);
int dict_del_string_key(Dictionary *d, const void *string_key);
int dict_del_interned_string(Dictionary *d, const void *interned_key);
int dict_contains(const Dictionary *d, const char *key);
size_t dict_length(const Dictionary *d);
const Dict_Entry *dict_iter(const Dictionary *d, size_t *index);
void dict_free(Dictionary *d);
void dict_reverse(Dictionary *d);
const Dict_Entry *dict_min(const Dictionary *d, DictComparator comp);
const Dict_Entry *dict_max(const Dictionary *d, DictComparator comp);
void dict_sort(Dictionary *d, DictComparator comp);

#endif /* DICT_H */
