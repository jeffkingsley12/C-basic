#include "string_lib.h"
#include <string.h>
#include <limits.h>
#include <stdint.h>

static void* find_bytes(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen) {
    if (needlelen == 0) return (void*)haystack;
    if (haystacklen < needlelen) return NULL;

    const unsigned char *h = (const unsigned char*)haystack;
    const unsigned char *n = (const unsigned char*)needle;
    size_t limit = haystacklen - needlelen;
    unsigned char first = n[0];
    unsigned char last = n[needlelen - 1];

    for (size_t i = 0; i <= limit; i++) {
        const unsigned char *p = (const unsigned char*)memchr(h + i, first, limit - i + 1);
        if (!p) return NULL;
        i = (size_t)(p - h);
        if (p[needlelen - 1] == last && memcmp(p, n, needlelen) == 0) {
            return (void*)p;
        }
    }
    return NULL;
}

static String empty_string_singleton = {
    .length = 0,
    .allocator = NULL,
    .hash = 2166136261u,
    .refcount = UINT32_MAX / 2,
    .data = {'\0'}
};

String* string_empty(Allocator *alloc) {
    (void)alloc;
    return &empty_string_singleton;
}

String* string_create_with_len(const char *src, size_t len, Allocator *alloc) {
    if (!alloc || !alloc->alloc) return NULL;
    if (len > SIZE_MAX - sizeof(String) - 1) return NULL;

    size_t total_size = sizeof(String) + len + 1;
    String *s = (String*)alloc->alloc(NULL, total_size, alloc->context);
    if (!s) return NULL;

    s->length = len;
    s->allocator = alloc;
    atomic_init(&s->hash, 0);
    atomic_init(&s->refcount, 1);

    if (src && len > 0) {
        memcpy(s->data, src, len);
    }
    s->data[len] = '\0';
    return s;
}

String* string_create(const char *src, Allocator *alloc) {
    return string_create_with_len(src, src ? strlen(src) : 0, alloc);
}

String* string_ref(String *s) {
    if (s && s != &empty_string_singleton) {
        atomic_fetch_add_explicit(&s->refcount, 1, memory_order_relaxed);
    }
    return s;
}

void string_unref(String *s) {
    if (!s || s == &empty_string_singleton) return;
    if (atomic_fetch_sub_explicit(&s->refcount, 1, memory_order_acq_rel) == 1) {
        s->allocator->alloc(s, 0, s->allocator->context);
    }
}

uint32_t string_hash(const String *s) {
    if (!s) return 0;
    uint32_t h = atomic_load_explicit(&s->hash, memory_order_acquire);
    if (h != 0) {
        return h;
    }

    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < s->length; i++) {
        hash ^= (unsigned char)s->data[i];
        hash *= 16777619u;
    }
    if (hash == 0) hash = 1;

    String *mut_s = (String *)s;
    atomic_store_explicit(&mut_s->hash, hash, memory_order_release);
    return hash;
}

uint32_t string_hash_void(const void *s) {
    return string_hash((const String*)s);
}

bool string_equal(const String *s1, const String *s2) {
    if (s1 == s2) return true;
    if (!s1 || !s2) return false;
    if (s1->length != s2->length) return false;
    if (string_hash(s1) != string_hash(s2)) return false;
    return memcmp(s1->data, s2->data, s1->length) == 0;
}

bool string_equal_void(const void *s1, const void *s2) {
    return string_equal((const String*)s1, (const String*)s2);
}

int string_compare(const String *s1, const String *s2) {
    if (s1 == s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;

    size_t min_len = s1->length < s2->length ? s1->length : s2->length;
    int cmp = memcmp(s1->data, s2->data, min_len);
    if (cmp != 0) return cmp;
    return (s1->length > s2->length) - (s1->length < s2->length);
}

intptr_t string_find(const String *s, const String *sub, size_t start) {
    if (!s || !sub || start > s->length) return -1;
    if (sub->length == 0) return (intptr_t)start;

    void *match = find_bytes(s->data + start, s->length - start, sub->data, sub->length);
    if (!match) return -1;
    return (intptr_t)((const char *)match - s->data);
}

intptr_t string_rfind(const String *s, const String *sub, intptr_t start) {
    if (!s || !sub) return -1;

    intptr_t len = (intptr_t)s->length;
    if (start < 0) start += len + 1;
    if (start < 0) start = 0;
    if (start > len) start = len;

    if (sub->length == 0) return start;
    if (sub->length > (size_t)start) return -1;

    for (intptr_t i = start - (intptr_t)sub->length; i >= 0; i--) {
        if (memcmp(s->data + i, sub->data, sub->length) == 0) {
            return i;
        }
    }
    return -1;
}

bool string_contains(const String *s, const String *sub) {
    return string_find(s, sub, 0) != -1;
}

bool string_starts_with(const String *s, const String *prefix) {
    if (!s || !prefix || s->length < prefix->length) return false;
    return memcmp(s->data, prefix->data, prefix->length) == 0;
}

bool string_ends_with(const String *s, const String *suffix) {
    if (!s || !suffix || s->length < suffix->length) return false;
    size_t offset = s->length - suffix->length;
    return memcmp(s->data + offset, suffix->data, suffix->length) == 0;
}

String* string_slice(const String *s, intptr_t start, intptr_t end) {
    if (!s) return NULL;
    intptr_t len = (intptr_t)s->length;

    if (start < 0) start += len;
    if (end < 0) end += len;
    if (start < 0) start = 0;
    if (start > len) start = len;
    if (end < 0) end = 0;
    if (end > len) end = len;

    if (start >= end) return &empty_string_singleton;
    return string_create_with_len(s->data + start, (size_t)(end - start), s->allocator);
}

String* string_concat(const String *s1, const String *s2) {
    if (!s1 || !s2) return NULL;
    if (s1->length > SIZE_MAX - s2->length) return NULL;

    Allocator *alloc = s1->allocator ? s1->allocator : s2->allocator;
    if (!alloc) return &empty_string_singleton;

    size_t combined_len = s1->length + s2->length;
    String *res = string_create_with_len(NULL, combined_len, alloc);
    if (!res) return NULL;

    memcpy(res->data, s1->data, s1->length);
    memcpy(res->data + s1->length, s2->data, s2->length);
    return res;
}

String* string_repeat(const String *s, size_t count) {
    if (!s) return NULL;
    if (count == 0 || s->length == 0) return &empty_string_singleton;
    if (s->length > SIZE_MAX / count) return NULL;

    size_t combined_len = s->length * count;
    String *res = string_create_with_len(NULL, combined_len, s->allocator);
    if (!res) return NULL;

    memcpy(res->data, s->data, s->length);
    size_t copied = s->length;
    while (copied <= combined_len / 2) {
        memcpy(res->data + copied, res->data, copied);
        copied *= 2;
    }
    if (copied < combined_len) {
        memcpy(res->data + copied, res->data, combined_len - copied);
    }
    return res;
}

String* string_replace(const String *s, const String *old_sub, const String *new_sub) {
    if (!s || !old_sub || !new_sub) return NULL;
    if (s->length == 0 || old_sub->length == 0) return string_ref((String*)s);

    size_t match_count = 0;
    intptr_t pos = 0;
    while ((pos = string_find(s, old_sub, (size_t)pos)) != -1) {
        match_count++;
        pos += (intptr_t)old_sub->length;
    }

    if (match_count == 0) return string_ref((String*)s);

    if (new_sub->length > 0 && match_count > SIZE_MAX / new_sub->length) return NULL;
    size_t added_len = match_count * new_sub->length;
    size_t removed_len = match_count * old_sub->length;
    if (s->length - removed_len > SIZE_MAX - added_len) return NULL;
    size_t new_len = (s->length - removed_len) + added_len;

    if (new_len == 0) return &empty_string_singleton;

    String *res = string_create_with_len(NULL, new_len, s->allocator);
    if (!res) return NULL;

    char *dest = res->data;
    size_t read_idx = 0;
    while ((pos = string_find(s, old_sub, read_idx)) != -1) {
        size_t chunk_size = (size_t)pos - read_idx;
        memcpy(dest, s->data + read_idx, chunk_size);
        dest += chunk_size;
        memcpy(dest, new_sub->data, new_sub->length);
        dest += new_sub->length;
        read_idx = (size_t)pos + old_sub->length;
    }
    memcpy(dest, s->data + read_idx, s->length - read_idx);
    return res;
}

StringArray string_split(const String *s, const String *sep, Allocator *alloc) {
    StringArray arr = {NULL, 0, alloc};
    if (!s || !sep || !alloc || !alloc->alloc) return arr;

    size_t cap = 4;
    arr.strings = (String**)alloc->alloc(NULL, cap * sizeof(String*), alloc->context);
    if (!arr.strings) return arr;

    size_t pos = 0;
    while (pos <= s->length) {
        intptr_t next = (sep->length == 0) ? (intptr_t)s->length : string_find(s, sep, pos);
        if (next == -1) {
            next = (intptr_t)s->length;
        }

        if (arr.count >= cap) {
            if (cap > SIZE_MAX / (2 * sizeof(String*))) {
                string_array_free(&arr);
                return (StringArray){NULL, 0, alloc};
            }
            cap *= 2;
            String **new_strings = (String**)alloc->alloc(arr.strings, cap * sizeof(String*), alloc->context);
            if (!new_strings) {
                string_array_free(&arr);
                return (StringArray){NULL, 0, alloc};
            }
            arr.strings = new_strings;
        }

        String *token = string_create_with_len(s->data + pos, (size_t)next - pos, alloc);
        if (!token) {
            string_array_free(&arr);
            return (StringArray){NULL, 0, alloc};
        }

        arr.strings[arr.count++] = token;
        if (next == (intptr_t)s->length || sep->length == 0) break;
        pos = (size_t)next + sep->length;
    }
    return arr;
}

void string_array_free(StringArray *arr) {
    if (!arr || !arr->strings) return;
    for (size_t i = 0; i < arr->count; i++) {
        string_unref(arr->strings[i]);
    }
    if (arr->alloc && arr->alloc->alloc) {
        arr->alloc->alloc(arr->strings, 0, arr->alloc->context);
    }
    arr->strings = NULL;
    arr->count = 0;
}

String* string_join(const String *sep, const StringArray *arr, Allocator *alloc) {
    if (!sep || !arr || !alloc) return NULL;
    if (arr->count == 0 || !arr->strings) return &empty_string_singleton;
    if (arr->count == 1 && arr->strings[0]) return string_ref(arr->strings[0]);

    size_t total_len = 0;
    for (size_t i = 0; i < arr->count; i++) {
        if (!arr->strings[i]) continue;
        if (arr->strings[i]->length > SIZE_MAX - total_len) return NULL;
        total_len += arr->strings[i]->length;
    }

    if (sep->length > 0) {
        if (sep->length > SIZE_MAX / (arr->count - 1)) return NULL;
        size_t sep_len = sep->length * (arr->count - 1);
        if (total_len > SIZE_MAX - sep_len) return NULL;
        total_len += sep_len;
    }

    if (total_len == 0) return &empty_string_singleton;

    String *res = string_create_with_len(NULL, total_len, alloc);
    if (!res) return NULL;

    char *dest = res->data;
    for (size_t i = 0; i < arr->count; i++) {
        if (arr->strings[i] && arr->strings[i]->length > 0) {
            memcpy(dest, arr->strings[i]->data, arr->strings[i]->length);
            dest += arr->strings[i]->length;
        }
        if (i < arr->count - 1 && sep->length > 0) {
            memcpy(dest, sep->data, sep->length);
            dest += sep->length;
        }
    }
    return res;
}

size_t string_len(const String *s) { return s ? s->length : 0; }
const char* string_c_str(const String *s) { return s ? s->data : ""; }
