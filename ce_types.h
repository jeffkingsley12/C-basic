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

/* -------------------------------------------------------------------------- */
/* Cross-Compiler Alignment Macros                                           */
/* -------------------------------------------------------------------------- */

#if defined(_MSC_VER)
    #define CE_ALIGN_DECL(n) __declspec(align(n))
    #define CE_ALIGN_ATTR(n)
#elif defined(__GNUC__) || defined(__clang__)
    #define CE_ALIGN_DECL(n)
    #define CE_ALIGN_ATTR(n) __attribute__((aligned(n)))
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #include <stdalign.h>
    #define CE_ALIGN_DECL(n) _Alignas(n)
    #define CE_ALIGN_ATTR(n)
#else
    #define CE_ALIGN_DECL(n)
    #define CE_ALIGN_ATTR(n)
#endif

/* -------------------------------------------------------------------------- */
/* Common Engine Utility Macros                                               */
/* -------------------------------------------------------------------------- */

#define CE_KB(x) ((ce_usize)(x) << 10)
#define CE_MB(x) ((ce_usize)(x) << 20)
#define CE_GB(x) ((ce_usize)(x) << 30)

#define CE_ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

/* -------------------------------------------------------------------------- */
/* Fundamental Integral Types                                                 */
/* -------------------------------------------------------------------------- */

/* Signed Integers */
typedef int8_t   ce_i8;   /* 1 Byte  | Min: -128 | Max: 127 */
typedef int16_t  ce_i16;  /* 2 Bytes | Min: -32,768 | Max: 32,767 */
typedef int32_t  ce_i32;  /* 4 Bytes | Min: -2,147,483,648 | Max: 2,147,483,647 */
typedef int64_t  ce_i64;  /* 8 Bytes | Min: -9,223,372,036,854,775,808 | Max: 9,223,372,036,854,775,807 */

/* Unsigned Integers */
typedef uint8_t  ce_u8;   /* 1 Byte  | Min: 0 | Max: 255 */
typedef uint16_t ce_u16;  /* 2 Bytes | Min: 0 | Max: 65,535 */
typedef uint32_t ce_u32;  /* 4 Bytes | Min: 0 | Max: 4,294,967,295 */
typedef uint64_t ce_u64;  /* 8 Bytes | Min: 0 | Max: 18,446,744,073,709,551,615 */

/* Explicit Byte Type for binary data buffers, distinguishing intent from math */
typedef ce_u8 ce_byte;

/* -------------------------------------------------------------------------- */
/* Boolean Types                                                              */
/* -------------------------------------------------------------------------- */

typedef bool   ce_b8;   /* 1-byte boolean for tightly packed structures */
typedef ce_i32 ce_b32;  /* 4-byte alignment-friendly boolean. Never compare to true or 1. */

/* -------------------------------------------------------------------------- */
/* Floating Point Types                                                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief Wrapped 16-bit half-precision float bit pattern.
 * Wrapped in a struct to explicitly block accidental compiler language math evaluations.
 * Intended strictly for asset stream optimization and GPU storage format.
 */
typedef struct { ce_u16 bits; } ce_f16_bits;

typedef float  ce_f32;  /* 4-byte single-precision IEEE 754 float */
typedef double ce_f64;  /* 8-byte double-precision IEEE 754 float */

/* -------------------------------------------------------------------------- */
/* Pointer & Size Types                                                       */
/* -------------------------------------------------------------------------- */

typedef intptr_t  ce_isize; 
typedef uintptr_t ce_usize; 
typedef size_t    ce_sz;    

/* -------------------------------------------------------------------------- */
/* Character & String Types                                                   */
/* -------------------------------------------------------------------------- */

typedef char     ce_c8;     /* Standard 8-bit ASCII/UTF-8 character code unit */
typedef char16_t ce_char16; /* 16-bit Unicode character code unit (UTF-16) */
typedef char32_t ce_char32; /* 32-bit Unicode character code unit (UTF-32) */

/** * @note Portability Warning: Size is platform-dependent (2 bytes on Windows, 4 bytes on Linux). 
 * Limit utilization exclusively to native operating system API bridges.
 */
typedef wchar_t  ce_wc;     

/**
 * @brief Zero-allocation string view slice.
 * Prevents mutable pointer expansion bugs and eliminates redundant heap allocations.
 */
typedef struct {
    const ce_c8* data;   /* Pointer to the start of the character array segment */
    ce_usize     length; /* Length of the string slice, ignoring null terminators */
} ce_string_view;

/* -------------------------------------------------------------------------- */
/* Mathematical Vector & Spatial Primitives                                   */
/* -------------------------------------------------------------------------- */

typedef struct { ce_f32 x; ce_f32 y; } ce_vec2f;
typedef struct { ce_f32 x; ce_f32 y; ce_f32 z; } ce_vec3f;

/* Aligned to 16 bytes to enable safe hardware SIMD load/store execution paths */
typedef struct CE_ALIGN_DECL(16) {
    ce_f32 x, y, z, w;
} CE_ALIGN_ATTR(16) ce_vec4f;

typedef struct { ce_i32 x; ce_i32 y; } ce_vec2i;
typedef struct { ce_i32 x; ce_i32 y; ce_i32 z; } ce_vec3i;

/* -------------------------------------------------------------------------- */
/* Explicit Hardware SIMD Register Types                                      */
/* -------------------------------------------------------------------------- */

#if defined(__SSE__) || defined(_M_AMD64) || defined(_M_X64)
    #include <xmmintrin.h>
    typedef __m128 ce_simd4f;
    #define CE_SIMD_SUPPORTED 1
#elif defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(_M_ARM64)
    #include <arm_neon.h>
    typedef float32x4_t ce_simd4f;
    #define CE_SIMD_SUPPORTED 1
#else
    typedef struct CE_ALIGN_DECL(16) { ce_f32 v[4]; } CE_ALIGN_ATTR(16) ce_simd4f;
    #define CE_SIMD_SUPPORTED 0
#endif

/* -------------------------------------------------------------------------- */
/* Matrices & Rotations                                                       */
/* -------------------------------------------------------------------------- */

typedef struct { ce_f32 m[2][2]; } ce_mat2;
typedef struct { ce_f32 m[3][3]; } ce_mat3;

/* Aligned to 16 bytes for linear math vector operations. Format is Column-Major: [col][row] */
typedef struct CE_ALIGN_DECL(16) {
    ce_f32 m[4][4];
} CE_ALIGN_ATTR(16) ce_mat4;

/* Quaternions for orientation representation without gimbal lock */
typedef struct CE_ALIGN_DECL(16) {
    ce_f32 x, y, z, w;
} CE_ALIGN_ATTR(16) ce_quat;

/* -------------------------------------------------------------------------- */
/* Spatial Volumes & Scene Graph Elements                                      */
/* -------------------------------------------------------------------------- */

/* Axis-Aligned Bounding Box for fast culling operations and broad-phase physics */
typedef struct {
    ce_vec3f min;
    ce_vec3f max;
} ce_aabb;

/* Infinite mathematical plane for frustum extraction and intersection routines */
typedef struct {
    ce_vec3f normal;
    ce_f32   distance; /* Signed distance from the coordinate origin along the normal */
} ce_plane;

/**
 * @brief Spatial placement configuration within coordinate spaces.
 * Explicitly aligned to 16 bytes. Sorted by type alignment footprint to eliminate 
 * silent internal padding holes. Total structural memory size = 48 bytes.
 */
typedef struct CE_ALIGN_DECL(16) {
    ce_quat  rotation; /* 16 bytes */
    ce_vec3f position; /* 12 bytes */
    ce_vec3f scale;    /* 12 bytes */
} CE_ALIGN_ATTR(16) ce_transform;

/* -------------------------------------------------------------------------- */
/* Graphics & Colors                                                          */
/* -------------------------------------------------------------------------- */

/* Floating point color layout aligned to 16 bytes for rapid GPU Constant Buffer uploads */
typedef struct CE_ALIGN_DECL(16) {
    ce_f32 r, g, b, a;
} CE_ALIGN_ATTR(16) ce_colorf;

/* Packaged 32-bit color value. Use specific macros for system-endian manipulation. */
typedef ce_u32 ce_color32; 

/* -------------------------------------------------------------------------- */
/* Robust Serialization & Identification Structures                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Split ECS Entity handle.
 * Combines an active lookup array index with an incremental recycling generation ID
 * to eliminate the stale handle / ABA mutation defect.
 */
typedef struct {
    ce_u32 index;
    ce_u32 generation;
} ce_entity;

typedef struct { ce_u64 id; } ce_asset_id;

/* 128-bit globally unique structural identifier to scale distributed systems seamlessly */
typedef struct {
    ce_u64 high;
    ce_u64 low;
} ce_guid;

/* Standardized semantic version format with explicit layout padding to prevent data leaks */
typedef struct {
    ce_u16 major;
    ce_u16 minor;
    ce_u16 patch;
    ce_u16 reserved; /* Explicit layout padding to guarantee clean 8-byte structure footprint */
} ce_version;

/* Handle Initialization Constants */
#define CE_INVALID_ENTITY   ((ce_entity){ .index = 0xFFFFFFFF, .generation = 0 })
#define CE_INVALID_ASSET_ID ((ce_asset_id){ 0 })
#define CE_INVALID_GUID     ((ce_guid){ 0, 0 })

/* -------------------------------------------------------------------------- */
/* System Time Formats                                                        */
/* -------------------------------------------------------------------------- */

typedef ce_f64 ce_seconds;     /* Double-precision system delta timing tracking metrics */
typedef ce_u64 ce_nanoseconds; /* High-precision absolute system time measurements */

/**
 * @note Platform Specific: Represents raw hardware execution cycle counts (e.g., RDTSC).
 * Do not assume a direct correlation to real-world calendar time without calibration.
 */
typedef ce_u64 ce_ticks;       

/* -------------------------------------------------------------------------- */
/* Compile-Time Static Validation Checks                                      */
/* -------------------------------------------------------------------------- */

_Static_assert(sizeof(ce_i8)   == 1, "ce_i8 size invalid");
_Static_assert(sizeof(ce_i16)  == 2, "ce_i16 size invalid");
_Static_assert(sizeof(ce_i32)  == 4, "ce_i32 size invalid");
_Static_assert(sizeof(ce_i64)  == 8, "ce_i64 size invalid");
_Static_assert(sizeof(ce_u8)   == 1, "ce_u8 size invalid");
_Static_assert(sizeof(ce_u16)  == 2, "ce_u16 size invalid");
_Static_assert(sizeof(ce_u32)  == 4, "ce_u32 size invalid");
_Static_assert(sizeof(ce_u64)  == 8, "ce_u64 size invalid");

_Static_assert(sizeof(ce_f16_bits) == 2, "ce_f16_bits size layout allocation invalid");
_Static_assert(sizeof(ce_f32)      == 4, "ce_f32 size invalid");
_Static_assert(sizeof(ce_f64)      == 8, "ce_f64 size invalid");

_Static_assert(sizeof(ce_vec4f)     == 16, "ce_vec4f SIMD alignment constraint violation");
_Static_assert(sizeof(ce_mat4)      == 64, "ce_mat4 SIMD alignment constraint violation");
_Static_assert(sizeof(ce_quat)      == 16, "ce_quat SIMD alignment constraint violation");
_Static_assert(sizeof(ce_simd4f)    == 16, "ce_simd4f platform alignment layout mismatch");
_Static_assert(sizeof(ce_colorf)    == 16, "ce_colorf graphics hardware boundary alignment violation");
_Static_assert(sizeof(ce_version)   == 8,  "ce_version structural memory leakage padding detected");
_Static_assert(sizeof(ce_transform) == 48, "ce_transform internal padding layout holes detected");

#endif /* CE_TYPES_H */