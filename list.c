#include "list.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

static size_t calculate_new_capacity(size_t current_capacity) {
    size_t delta = (current_capacity >> 3) + (current_capacity < 9 ? 3 : 6);
    if (current_capacity > SIZE_MAX - delta) {
        return SIZE_MAX;
    }
    size_t new_cap = current_capacity + delta;
    if (new_cap <= current_capacity) {
        new_cap = current_capacity + 1;
    }
    return new_cap;
}

List* list_create(size_t initial_capacity, Allocator *allocator, ListItemRefCallback ref_callback) {
    if (!allocator || !allocator->alloc) return NULL;
    if (initial_capacity == 0) initial_capacity = 8;
    if (initial_capacity > SIZE_MAX / sizeof(void*)) return NULL;

    List *list = (List*)allocator->alloc(NULL, sizeof(List), allocator->context);
    if (!list) return NULL;

    list->items = (void**)allocator->alloc(NULL, initial_capacity * sizeof(void*), allocator->context);
    if (!list->items) {
        allocator->alloc(list, 0, allocator->context);
        return NULL;
    }

    list->length = 0;
    list->capacity = initial_capacity;
    list->allocator = allocator;
    list->ref_callback = ref_callback;
    return list;
}

static int ensure_capacity(List *list, size_t required) {
    if (required <= list->capacity) return 1;

    size_t new_capacity = calculate_new_capacity(list->capacity);
    while (new_capacity < required) {
        size_t next = calculate_new_capacity(new_capacity);
        if (next <= new_capacity) return 0;
        new_capacity = next;
    }

    if (new_capacity > SIZE_MAX / sizeof(void*)) return 0;

    void **new_items = (void**)list->allocator->alloc(
        list->items, new_capacity * sizeof(void*), list->allocator->context);
    if (!new_items) return 0;

    list->items = new_items;
    list->capacity = new_capacity;
    return 1;
}

int list_append(List *list, void *item) {
    if (!list || list->length == SIZE_MAX) return 0;
    if (!ensure_capacity(list, list->length + 1)) return 0;
    list->items[list->length++] = item;
    return 1;
}

int list_insert(List *list, size_t index, void *item) {
    if (!list || index > list->length || list->length == SIZE_MAX) return 0;
    if (!ensure_capacity(list, list->length + 1)) return 0;

    if (index < list->length) {
        memmove(&list->items[index + 1], &list->items[index],
                (list->length - index) * sizeof(void*));
    }
    list->items[index] = item;
    list->length++;
    return 1;
}

void* list_remove(List *list, size_t index) {
    if (!list || index >= list->length) return NULL;
    void *item = list->items[index];

    if (index + 1 < list->length) {
        memmove(&list->items[index], &list->items[index + 1],
                (list->length - index - 1) * sizeof(void*));
    }
    list->length--;
    if (list->ref_callback && item) {
        list->ref_callback(item, list->allocator);
    }
    return item;
}

void* list_pop(List *list) {
    if (!list || list->length == 0) return NULL;
    void *item = list->items[--list->length];
    if (list->ref_callback && item) {
        list->ref_callback(item, list->allocator);
    }
    return item;
}

void* list_get(const List *list, size_t index) {
    if (!list || index >= list->length) return NULL;
    return list->items[index];
}

int list_set(List *list, size_t index, void *item) {
    if (!list || index >= list->length) return 0;
    if (list->items[index] == item) return 1;

    if (list->ref_callback && list->items[index]) {
        list->ref_callback(list->items[index], list->allocator);
    }
    list->items[index] = item;
    return 1;
}

void list_clear(List *list) {
    if (!list) return;
    if (list->ref_callback) {
        for (size_t i = 0; i < list->length; i++) {
            if (list->items[i]) {
                list->ref_callback(list->items[i], list->allocator);
            }
        }
    }
    list->length = 0;
}

int list_resize(List *list, size_t new_capacity) {
    if (!list || new_capacity < list->length) return 0;
    if (new_capacity == 0) new_capacity = 1;
    if (new_capacity == list->capacity) return 1;
    if (new_capacity > SIZE_MAX / sizeof(void*)) return 0;

    void **new_items = (void**)list->allocator->alloc(
        list->items, new_capacity * sizeof(void*), list->allocator->context);
    if (!new_items) return 0;

    list->items = new_items;
    list->capacity = new_capacity;
    return 1;
}

size_t list_length(const List *list) {
    return list ? list->length : 0;
}

size_t list_capacity(const List *list) {
    return list ? list->capacity : 0;
}

void list_free(List *list) {
    if (!list) return;
    list_clear(list);
    list->allocator->alloc(list->items, 0, list->allocator->context);
    list->allocator->alloc(list, 0, list->allocator->context);
}
