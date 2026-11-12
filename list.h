#ifndef LIST_H
#define LIST_H

#include "vector.h"

/**
 * @file list.h
 * @brief High-Level Runtime List Object for Compiler/Interpreter Frontends
 *
 * This module provides a boxed, stable-handle dynamic array designed for
 * runtime objects (tokens, AST nodes, user-facing lists). Unlike vector.c
 * which uses hidden-header macros for infrastructure, list.c provides an
 * explicit List* handle that encapsulates memory movement and supports
 * reference counting hooks.
 *
 * Growth Strategy: CPython's exact non-linear formula for memory efficiency
 *   new_cap = (capacity >> 3) + (capacity < 9 ? 3 : 6) + capacity
 * This grows tightly at small sizes (conserving memory for short-lived lists)
 * while scaling defensively at large sizes.
 */

/**
 * @brief Reference-drop callback for list items
 * @param item The item whose reference the list is relinquishing (on remove, pop, clear, free, or overwrite in list_set)
 * @param allocator The allocator context
 *
 * Pass NULL to list_create if no container reference-drop hook is needed.
 */
typedef void (*ListItemRefCallback)(void *item, Allocator *allocator);

typedef struct {
    void **items;        // Array of generic object pointers
    size_t length;       // Current number of elements
    size_t capacity;     // Allocated capacity
    Allocator *allocator;
    ListItemRefCallback ref_callback;  // Optional reference counting callback
} List;

/**
 * @brief Create a new list with the specified initial capacity
 * @param initial_capacity Starting capacity (0 uses default of 8)
 * @param allocator Memory allocator
 * @param ref_callback Optional callback invoked when items are discarded (NULL for no ref counting)
 * @return New List* or NULL on allocation failure
 */
List* list_create(size_t initial_capacity, Allocator *allocator, ListItemRefCallback ref_callback);

/**
 * @brief Append an item to the end of the list
 * @param list The list to modify
 * @param item The item to append
 * @return 1 on success, 0 on allocation failure
 */
int list_append(List *list, void *item);

/**
 * @brief Insert an item at a specific index
 * @param list The list to modify
 * @param index Insertion position (0 <= index <= length)
 * @param item The item to insert
 * @return 1 on success, 0 on out-of-bounds or allocation failure
 */
int list_insert(List *list, size_t index, void *item);

/**
 * @brief Remove and return the item at a specific index
 * @param list The list to modify
 * @param index Position to remove from (0 <= index < length)
 * @return The removed item (invokes ref_callback as list drops its reference), or NULL on out-of-bounds
 */
void* list_remove(List *list, size_t index);

/**
 * @brief Pop the last item from the list
 * @param list The list to modify
 * @return The popped item (invokes ref_callback as list drops its reference), or NULL if list is empty
 */
void* list_pop(List *list);

/**
 * @brief Get the item at a specific index without removing it
 * @param list The list to query
 * @param index Position to query (0 <= index < length)
 * @return The item at index, or NULL on out-of-bounds
 */
void* list_get(const List *list, size_t index);

/**
 * @brief Set the item at a specific index (invokes ref_callback on old item if replaced)
 * @param list The list to modify
 * @param index Position to set (0 <= index < length)
 * @param item The new item
 * @return 1 on success, 0 on out-of-bounds
 */
int list_set(List *list, size_t index, void *item);

/**
 * @brief Clear all items from the list (invokes ref_callback for each item)
 * @param list The list to clear
 */
void list_clear(List *list);

/**
 * @brief Resize the list to a new capacity
 * @param list The list to resize
 * @param new_capacity The new capacity (must be >= length)
 * @return 1 on success, 0 on allocation failure
 */
int list_resize(List *list, size_t new_capacity);

/**
 * @brief Get the current length of the list
 * @param list The list to query
 * @return Number of elements in the list
 */
size_t list_length(const List *list);

/**
 * @brief Get the current capacity of the list
 * @param list The list to query
 * @return Allocated capacity
 */
size_t list_capacity(const List *list);

/**
 * @brief Free the list and all associated memory (invokes ref_callback for each remaining item)
 * @param list The list to free
 */
void list_free(List *list);

#endif
