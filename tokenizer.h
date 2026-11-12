#ifndef STRING_POOL_H
#define STRING_POOL_H

#include "ce_types.h"
#include "dict.h"
#include "string_lib.h"

typedef struct {
    Dictionary *map;
    Allocator *allocator;
} StringPool;

StringPool *string_pool_create(Allocator *allocator);
String *string_pool_intern(StringPool *pool, String *str);
void string_pool_free(StringPool *pool);

#endif /* STRING_POOL_H */
