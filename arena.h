#ifndef CE_TYPES_H
#define CE_TYPES_H

/**
 * @file ce_types.h
 * @brief Production-grade core type architecture for a high-performance C engine.
 * Enforces strict alignment, data isolation, and type-safe abstractions.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <uchar.h>

#if defined(_MSC_VER)
    #define ALIGN_DECL(n) __declspec(align(n))
    #define ALIGN_ATTR(n)
#elif defined(__GNUC__) || defined(__clang__)
    #define ALIGN_DECL(n)
    #define ALIGN_ATTR(n) __attribute__((aligned(n)))
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #include <stdalign.h>
    #define ALIGN_DECL(n) _Alignas(n)
    #define ALIGN_ATTR(n)
#else
    #define ALIGN_DECL(n)
    #define ALIGN_ATTR(n)
#endif

#define KB(x) ((usize)(x) << 10)
#define MB(x) ((usize)(x) << 20)
#define GB(x) ((usize)(x) << 30)
#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef u8 byte;
typedef bool b8;
typedef i32 b32;
typedef struct { u16 bits; } f16_bits;
typedef float f32;
typedef double f64;
typedef intptr_t isize;
typedef uintptr_t usize;
typedef size_t sz;
typedef char c8;
typedef char16_t char16;
typedef char32_t char32;
typedef wchar_t wc;
typedef struct {
    const c8 *data;
    usize length;
} string_view;

typedef struct { f32 x; f32 y; } vec2f;
typedef struct { f32 x; f32 y; f32 z; } vec3f;
typedef struct ALIGN_DECL(16) { f32 x, y, z, w; } ALIGN_ATTR(16) vec4f;
typedef struct { i32 x; i32 y; } vec2i;
typedef struct { i32 x; i32 y; i32 z; } vec3i;

#if defined(__SSE__) || defined(_M_AMD64) || defined(_M_X64)
    #include <xmmintrin.h>
    typedef __m128 simd4f;
    #define SIMD_SUPPORTED 1
#elif defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(_M_ARM64)
    #include <arm_neon.h>
    typedef float32x4_t simd4f;
    #define SIMD_SUPPORTED 1
#else
    typedef struct ALIGN_DECL(16) { f32 v[4]; } ALIGN_ATTR(16) simd4f;
    #define SIMD_SUPPORTED 0
#endif

typedef struct { f32 m[2][2]; } mat2;
typedef struct { f32 m[3][3]; } mat3;
typedef struct ALIGN_DECL(16) { f32 m[4][4]; } ALIGN_ATTR(16) mat4;
typedef struct ALIGN_DECL(16) { f32 x, y, z, w; } ALIGN_ATTR(16) quat;
typedef struct { vec3f min; vec3f max; } aabb;
typedef struct { vec3f normal; f32 distance; } plane;
typedef struct ALIGN_DECL(16) { quat rotation; vec3f position; vec3f scale; } ALIGN_ATTR(16) transform;
typedef struct ALIGN_DECL(16) { f32 r, g, b, a; } ALIGN_ATTR(16) colorf;
typedef u32 color32;
typedef struct { u32 index; u32 generation; } entity;
typedef struct { u64 id; } asset_id;
typedef struct { u64 high; u64 low; } guid;
typedef struct { u16 major; u16 minor; u16 patch; u16 reserved; } version;

#define INVALID_ENTITY   ((entity){ .index = 0xFFFFFFFF, .generation = 0 })
#define INVALID_ASSET_ID ((asset_id){ 0 })
#define INVALID_GUID     ((guid){ 0, 0 })

typedef f64 seconds;
typedef u64 nanoseconds;
typedef u64 ticks;

_Static_assert(sizeof(i8)       == 1, "i8 size invalid");
_Static_assert(sizeof(i16)      == 2, "i16 size invalid");
_Static_assert(sizeof(i32)      == 4, "i32 size invalid");
_Static_assert(sizeof(i64)      == 8, "i64 size invalid");
_Static_assert(sizeof(u8)       == 1, "u8 size invalid");
_Static_assert(sizeof(u16)      == 2, "u16 size invalid");
_Static_assert(sizeof(u32)      == 4, "u32 size invalid");
_Static_assert(sizeof(u64)      == 8, "u64 size invalid");
_Static_assert(sizeof(f16_bits) == 2, "f16_bits size layout allocation invalid");
_Static_assert(sizeof(f32)      == 4, "f32 size invalid");
_Static_assert(sizeof(f64)      == 8, "f64 size invalid");
_Static_assert(sizeof(vec4f)     == 16, "vec4f SIMD alignment constraint violation");
_Static_assert(sizeof(mat4)     == 64, "mat4 SIMD alignment constraint violation");
_Static_assert(sizeof(quat)     == 16, "quat SIMD alignment constraint violation");
_Static_assert(sizeof(simd4f)   == 16, "simd4f platform alignment layout mismatch");
_Static_assert(sizeof(colorf)   == 16, "colorf graphics hardware boundary alignment violation");
_Static_assert(sizeof(version)  == 8,  "version structural memory leakage padding detected");
_Static_assert(sizeof(transform)== 48, "transform internal padding layout holes detected");

#endif /* CE_TYPES_H */
