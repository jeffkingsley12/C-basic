#ifndef LIST_H
#define LIST_H

#include <stddef.h>
#include "ce_types.h"
#include "vector.h"

typedef void (*ListItemRefCallback)(void *item, Allocator *allocator);
typedef struct {
    void **items;
    size_t length;
    size_t capacity;
    Allocator *allocator;
    ListItemRefCallback ref_callback;
} List;

List *list_create(size_t initial_capacity, Allocator *allocator, ListItemRefCallback ref_callback);
int list_append(List *list, void *item);
int list_insert(List *list, size_t index, void *item);
void *list_remove(List *list, size_t index);
void *list_pop(List *list);
void *list_get(const List *list, size_t index);
int list_set(List *list, size_t index, void *item);
void list_clear(List *list);
int list_resize(List *list, size_t new_capacity);
size_t list_length(const List *list);
size_t list_capacity(const List *list);
void list_free(List *list);

#endif /* LIST_H */
