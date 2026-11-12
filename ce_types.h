#ifndef TYPES_H
#define TYPES_H

/**
 * @file ce_types.h
 * @brief Production-grade core type architecture for a high-performance C engine.
 * Enforces strict alignment, data isolation, and type-safe abstractions.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <uchar.h>

/* -------------------------------------------------------------------------- */
/* Cross-Compiler Alignment Macros                                           */
/* -------------------------------------------------------------------------- */

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

/* -------------------------------------------------------------------------- */
/* Common Engine Utility Macros                                               */
/* -------------------------------------------------------------------------- */

#define KB(x) ((usize)(x) << 10)
#define MB(x) ((usize)(x) << 20)
#define GB(x) ((usize)(x) << 30)

#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

/* -------------------------------------------------------------------------- */
/* Fundamental Integral Types                                                 */
/* -------------------------------------------------------------------------- */

/* Signed Integers */
typedef int8_t   i8;   /* 1 Byte  | Min: -128 | Max: 127 */
typedef int16_t  i16;  /* 2 Bytes | Min: -32,768 | Max: 32,767 */
typedef int32_t  i32;  /* 4 Bytes | Min: -2,147,483,648 | Max: 2,147,483,647 */
typedef int64_t  i64;  /* 8 Bytes | Min: -9,223,372,036,854,775,808 | Max: 9,223,372,036,854,775,807 */

/* Unsigned Integers */
typedef uint8_t  u8;   /* 1 Byte  | Min: 0 | Max: 255 */
typedef uint16_t u16;  /* 2 Bytes | Min: 0 | Max: 65,535 */
typedef uint32_t u32;  /* 4 Bytes | Min: 0 | Max: 4,294,967,295 */
typedef uint64_t u64;  /* 8 Bytes | Min: 0 | Max: 18,446,744,073,709,551,615 */

/* Explicit Byte Type for binary data buffers, distinguishing intent from math */
typedef u8 byte;

/* -------------------------------------------------------------------------- */
/* Boolean Types                                                              */
/* -------------------------------------------------------------------------- */

typedef bool   b8;   /* 1-byte boolean for tightly packed structures */
typedef i32    b32;  /* 4-byte alignment-friendly boolean. Never compare to true or 1. */

/* -------------------------------------------------------------------------- */
/* Floating Point Types                                                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief Wrapped 16-bit half-precision float bit pattern.
 * Wrapped in a struct to explicitly block accidental compiler language math evaluations.
 * Intended strictly for asset stream optimization and GPU storage format.
 */
typedef struct { u16 bits; } f16_bits;

typedef float  f32;  /* 4-byte single-precision IEEE 754 float */
typedef double f64;  /* 8-byte double-precision IEEE 754 float */

/* -------------------------------------------------------------------------- */
/* Pointer & Size Types                                                       */
/* -------------------------------------------------------------------------- */

typedef intptr_t  isize;
typedef uintptr_t usize;
typedef size_t    sz;

/* -------------------------------------------------------------------------- */
/* Character & String Types                                                   */
/* -------------------------------------------------------------------------- */

typedef char     c8;     /* Standard 8-bit ASCII/UTF-8 character code unit */
typedef char16_t char16; /* 16-bit Unicode character code unit (UTF-16) */
typedef char32_t char32; /* 32-bit Unicode character code unit (UTF-32) */

/** * @note Portability Warning: Size is platform-dependent (2 bytes on Windows, 4 bytes on Linux). 
 * Limit utilization exclusively to native operating system API bridges.
 */
typedef wchar_t  wc;

/**
 * @brief Zero-allocation string view slice.
 * Prevents mutable pointer expansion bugs and eliminates redundant heap allocations.
 */
typedef struct {
    const c8* data;   /* Pointer to the start of the character array segment */
    usize     length; /* Length of the string slice, ignoring null terminators */
} string_view;

/* -------------------------------------------------------------------------- */
/* Mathematical Vector & Spatial Primitives                                   */
/* -------------------------------------------------------------------------- */

typedef struct { f32 x; f32 y; } vec2f;
typedef struct { f32 x; f32 y; f32 z; } vec3f;

/* Aligned to 16 bytes to enable safe hardware SIMD load/store execution paths */
typedef struct ALIGN_DECL(16) {
    f32 x, y, z, w;
} ALIGN_ATTR(16) vec4f;

typedef struct { i32 x; i32 y; } vec2i;
typedef struct { i32 x; i32 y; i32 z; } vec3i;

/* -------------------------------------------------------------------------- */
/* Explicit Hardware SIMD Register Types                                      */
/* -------------------------------------------------------------------------- */

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

/* -------------------------------------------------------------------------- */
/* Matrices & Rotations                                                       */
/* -------------------------------------------------------------------------- */

typedef struct { f32 m[2][2]; } mat2;
typedef struct { f32 m[3][3]; } mat3;

/* Aligned to 16 bytes for linear math vector operations. Format is Column-Major: [col][row] */
typedef struct ALIGN_DECL(16) {
    f32 m[4][4];
} ALIGN_ATTR(16) mat4;

/* Quaternions for orientation representation without gimbal lock */
typedef struct ALIGN_DECL(16) {
    f32 x, y, z, w;
} ALIGN_ATTR(16) quat;

/* -------------------------------------------------------------------------- */
/* Spatial Volumes & Scene Graph Elements                                      */
/* -------------------------------------------------------------------------- */

/* Axis-Aligned Bounding Box for fast culling operations and broad-phase physics */
typedef struct {
    vec3f min;
    vec3f max;
} aabb;

/* Infinite mathematical plane for frustum extraction and intersection routines */
typedef struct {
    vec3f normal;
    f32   distance; /* Signed distance from the coordinate origin along the normal */
} plane;

/**
 * @brief Spatial placement configuration within coordinate spaces.
 * Explicitly aligned to 16 bytes. Sorted by type alignment footprint to eliminate 
 * silent internal padding holes. Total structural memory size = 48 bytes.
 */
typedef struct ALIGN_DECL(16) {
    quat  rotation; /* 16 bytes */
    vec3f position; /* 12 bytes */
    vec3f scale;    /* 12 bytes */
} ALIGN_ATTR(16) transform;

/* -------------------------------------------------------------------------- */
/* Graphics & Colors                                                          */
/* -------------------------------------------------------------------------- */

/* Floating point color layout aligned to 16 bytes for rapid GPU Constant Buffer uploads */
typedef struct ALIGN_DECL(16) {
    f32 r, g, b, a;
} ALIGN_ATTR(16) colorf;

/* Packaged 32-bit color value. Use specific macros for system-endian manipulation. */
typedef u32 color32;

/* -------------------------------------------------------------------------- */
/* Robust Serialization & Identification Structures                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Split ECS Entity handle.
 * Combines an active lookup array index with an incremental recycling generation ID
 * to eliminate the stale handle / ABA mutation defect.
 */
typedef struct {
    u32 index;
    u32 generation;
} entity;

typedef struct { u64 id; } asset_id;

/* 128-bit globally unique structural identifier to scale distributed systems seamlessly */
typedef struct {
    u64 high;
    u64 low;
} guid;

/* Standardized semantic version format with explicit layout padding to prevent data leaks */
typedef struct {
    u16 major;
    u16 minor;
    u16 patch;
    u16 reserved; /* Explicit layout padding to guarantee clean 8-byte structure footprint */
} version;

/* Handle Initialization Constants */
#define INVALID_ENTITY   ((entity){ .index = 0xFFFFFFFF, .generation = 0 })
#define INVALID_ASSET_ID ((asset_id){ 0 })
#define INVALID_GUID     ((guid){ 0, 0 })

/* -------------------------------------------------------------------------- */
/* System Time Formats                                                        */
/* -------------------------------------------------------------------------- */

typedef f64 seconds;      /* Double-precision system delta timing tracking metrics */
typedef u64 nanoseconds;  /* High-precision absolute system time measurements */

/**
 * @note Platform Specific: Represents raw hardware execution cycle counts (e.g., RDTSC).
 * Do not assume a direct correlation to real-world calendar time without calibration.
 */
typedef u64 ticks;

/* -------------------------------------------------------------------------- */
/* Compile-Time Static Validation Checks                                      */
/* -------------------------------------------------------------------------- */

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

_Static_assert(sizeof(vec4f)    == 16, "vec4f SIMD alignment constraint violation");
_Static_assert(sizeof(mat4)     == 64, "mat4 SIMD alignment constraint violation");
_Static_assert(sizeof(quat)     == 16, "quat SIMD alignment constraint violation");
_Static_assert(sizeof(simd4f)   == 16, "simd4f platform alignment layout mismatch");
_Static_assert(sizeof(colorf)   == 16, "colorf graphics hardware boundary alignment violation");
_Static_assert(sizeof(version)  == 8,  "version structural memory leakage padding detected");
_Static_assert(sizeof(transform)== 48, "transform internal padding layout holes detected");

#endif /* TYPES_H */
