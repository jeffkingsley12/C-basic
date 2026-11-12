#ifndef STRING_LIB_H
#define STRING_LIB_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "vector.h"

typedef struct {
    size_t length;                 
    Allocator *allocator;          
    _Atomic uint32_t hash;         // Atomic cached hash (0 = uncomputed)
    _Atomic uint32_t refcount;     // Atomic reference count
    char data[];                   
} String;

// Managed Immutable String Array Context
typedef struct {
    String **strings;
    size_t count;
    Allocator *alloc; 
} StringArray;

// Lifecycle Operations
String* string_create(const char *src, Allocator *alloc);
String* string_create_with_len(const char *src, size_t len, Allocator *alloc);
String* string_ref(String *s);
void    string_unref(String *s);
String* string_empty(Allocator *alloc);

// Core Inspection & Matching Primitives
size_t      string_len(const String *s);
const char* string_c_str(const String *s);
bool        string_equal(const String *s1, const String *s2);
bool        string_equal_void(const void *s1, const void *s2);
int         string_compare(const String *s1, const String *s2);
uint32_t    string_hash(const String *s);
uint32_t    string_hash_void(const void *s);

// Substring Search & Sequence Verification
intptr_t string_find(const String *s, const String *sub, size_t start);
intptr_t string_rfind(const String *s, const String *sub, intptr_t start);
bool     string_contains(const String *s, const String *sub);
bool     string_starts_with(const String *s, const String *prefix);
bool     string_ends_with(const String *s, const String *suffix);

// High-Level Transformations
String* string_concat(const String *s1, const String *s2);
String* string_slice(const String *s, intptr_t start, intptr_t end);
String* string_repeat(const String *s, size_t count);
String* string_replace(const String *s, const String *old_sub, const String *new_sub);

// Vector / Tokenization Utilities
StringArray string_split(const String *s, const String *sep, Allocator *alloc);
void        string_array_free(StringArray *arr);
String*     string_join(const String *sep, const StringArray *arr, Allocator *alloc);

#endif // STRING_LIB_H
