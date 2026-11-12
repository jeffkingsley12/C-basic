#ifndef CE_TYPES_H
#define CE_TYPES_H

/**
 * @file ce_types.h
 * @brief Production-grade core type architecture for a high-performance C engine.
 * Enforces strict alignment, data isolation, zero unnamed padding, and type-safe abstractions.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#if !defined(__cplusplus)
    #if defined(__has_include)
        #if __has_include(<uchar.h>)
            #include <uchar.h>
            #define CE_HAS_UCHAR_H 1
        #endif
    #elif !defined(__APPLE__)
        #include <uchar.h>
        #define CE_HAS_UCHAR_H 1
    #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Cross-Compiler Alignment & Static Assertion Macros                         */
/* -------------------------------------------------------------------------- */

/**
 * Note on ISO C11 _Alignas:
 * C11 (ISO/IEC 9899:2011 6.7.5) prohibits `_Alignas` directly on `struct` tags
 * (`typedef struct _Alignas(16) { ... }`) or `typedef` declarations.
 * `CE_ALIGN_MEMBER(n)` applies `_Alignas(n)` to the first struct field in pure C11,
 * while `CE_ALIGN_DECL(n)` / `CE_ALIGN_ATTR(n)` handle MSVC and GCC/Clang attributes.
 */
#if defined(_MSC_VER)
    #define CE_ALIGN_DECL(n)   __declspec(align(n))
    #define CE_ALIGN_ATTR(n)
    #define CE_ALIGN_MEMBER(n)
    #define CE_ALIGNOF(T)      __alignof(T)
#elif defined(__GNUC__) || defined(__clang__)
    #define CE_ALIGN_DECL(n)
    #define CE_ALIGN_ATTR(n)   __attribute__((aligned(n)))
    #define CE_ALIGN_MEMBER(n)
    #define CE_ALIGNOF(T)      __alignof__(T)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #include <stdalign.h>
    #define CE_ALIGN_DECL(n)
    #define CE_ALIGN_ATTR(n)
    #define CE_ALIGN_MEMBER(n) _Alignas(n)
    #define CE_ALIGNOF(T)      _Alignof(T)
#else
    #error "ce_types.h requires a compiler supporting 16-byte SIMD alignment (MSVC, GCC, Clang, or C11)."
#endif

#if defined(__cplusplus)
    #define CE_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define CE_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
    #define CE_STATIC_ASSERT(cond, msg) typedef char ce_static_assert_##__LINE__[(cond) ? 1 : -1]
#endif

/* -------------------------------------------------------------------------- */
/* Common Engine Utility Macros                                               */
/* -------------------------------------------------------------------------- */

/* Evaluated in 64-bit unsigned arithmetic to prevent 32-bit shift overflow on CE_GB(>=4) */
#define CE_KB(x) ((ce_u64)(x) << 10)
#define CE_MB(x) ((ce_u64)(x) << 20)
#define CE_GB(x) ((ce_u64)(x) << 30)

/* Rejects pointer decay at compile time on GCC/Clang so CE_ARRAY_COUNT(ptr) fails to build */
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__cplusplus)
    #define CE_MUST_BE_ARRAY(arr) \
        (sizeof(struct { int ce_not_an_array : (__builtin_types_compatible_p(__typeof__(arr), __typeof__(&(arr)[0])) ? -1 : 1); }) * 0u)
    #define CE_ARRAY_COUNT(arr) ((sizeof(arr) / sizeof((arr)[0])) + CE_MUST_BE_ARRAY(arr))
#else
    #define CE_ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

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

typedef intptr_t  ce_isize; /* Signed integer capable of holding a pointer */
typedef uintptr_t ce_usize; /* Unsigned integer capable of holding a pointer */
typedef size_t    ce_sz;    /* Object byte size / array count type */
typedef ptrdiff_t ce_isz;   /* Signed pointer difference type */

/* -------------------------------------------------------------------------- */
/* Character & String Types                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @note `char` signedness is platform-defined (signed on x86_64, unsigned on ARM Linux).
 * Always cast to `ce_u8` (`CE_U8_CHAR(c)`) before table indexing or >= 0x80 UTF-8 checks.
 */
typedef char ce_c8;
#define CE_U8_CHAR(c) ((ce_u8)(c))

#if defined(__cplusplus) || defined(CE_HAS_UCHAR_H)
    typedef char16_t ce_char16; /* 16-bit Unicode character code unit (UTF-16) */
    typedef char32_t ce_char32; /* 32-bit Unicode character code unit (UTF-32) */
#else
    typedef uint_least16_t ce_char16;
    typedef uint_least32_t ce_char32;
#endif

/**
 * @note Portability Warning: Size is platform-dependent (2 bytes on Windows, 4 bytes on Linux).
 * Limit utilization exclusively to native operating system API bridges.
 */
typedef wchar_t ce_wc;

/**
 * @brief Zero-allocation string view slice.
 * Prevents mutable pointer expansion bugs and eliminates redundant heap allocations.
 */
typedef struct {
    const ce_c8* data;   /* Pointer to the start of the character array segment */
    ce_sz        length; /* Length of the string slice in bytes, ignoring null terminators */
} ce_string_view;

#define CE_SV_LIT(lit) { (lit), sizeof(lit) - 1u }

/* -------------------------------------------------------------------------- */
/* Mathematical Vector & Spatial Primitives                                   */
/* -------------------------------------------------------------------------- */

typedef struct { ce_f32 x; ce_f32 y; } ce_vec2f;
typedef struct { ce_f32 x; ce_f32 y; ce_f32 z; } ce_vec3f;

/* Aligned to 16 bytes to enable safe hardware SIMD load/store execution paths */
typedef struct CE_ALIGN_DECL(16) {
    CE_ALIGN_MEMBER(16) ce_f32 x, y, z, w;
} CE_ALIGN_ATTR(16) ce_vec4f;

typedef struct { ce_i32 x; ce_i32 y; } ce_vec2i;
typedef struct { ce_i32 x; ce_i32 y; ce_i32 z; } ce_vec3i;
typedef struct { ce_i32 x; ce_i32 y; ce_i32 z; ce_i32 w; } ce_vec4i;

/* -------------------------------------------------------------------------- */
/* Explicit Hardware SIMD Register Types                                      */
/* -------------------------------------------------------------------------- */

#if defined(__SSE__) || defined(_M_AMD64) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
    #include <xmmintrin.h>
    typedef __m128 ce_simd4f;
    #define CE_SIMD_SUPPORTED 1
#elif defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(_M_ARM64)
    #include <arm_neon.h>
    typedef float32x4_t ce_simd4f;
    #define CE_SIMD_SUPPORTED 1
#elif defined(__wasm_simd128__)
    #include <wasm_simd128.h>
    typedef v128_t ce_simd4f;
    #define CE_SIMD_SUPPORTED 1
#else
    typedef struct CE_ALIGN_DECL(16) {
        CE_ALIGN_MEMBER(16) ce_f32 v[4];
    } CE_ALIGN_ATTR(16) ce_simd4f;
    #define CE_SIMD_SUPPORTED 0
#endif

/* -------------------------------------------------------------------------- */
/* Matrices & Rotations                                                       */
/* -------------------------------------------------------------------------- */

/* Aligned to 16 bytes so a 2x2 matrix (4 floats) loads in a single SIMD register */
typedef struct CE_ALIGN_DECL(16) {
    CE_ALIGN_MEMBER(16) ce_f32 m[2][2];
} CE_ALIGN_ATTR(16) ce_mat2;

typedef struct { ce_f32 m[3][3]; } ce_mat3;

/* Aligned to 16 bytes for linear math vector operations. Format is Column-Major: [col][row] */
typedef struct CE_ALIGN_DECL(16) {
    CE_ALIGN_MEMBER(16) ce_f32 m[4][4];
} CE_ALIGN_ATTR(16) ce_mat4;

/* Quaternions for orientation representation without gimbal lock */
typedef struct CE_ALIGN_DECL(16) {
    CE_ALIGN_MEMBER(16) ce_f32 x, y, z, w;
} CE_ALIGN_ATTR(16) ce_quat;

/* -------------------------------------------------------------------------- */
/* Spatial Volumes & Scene Graph Elements                                     */
/* -------------------------------------------------------------------------- */

/* Axis-Aligned Bounding Box for fast culling operations and broad-phase physics */
typedef struct {
    ce_vec3f min;
    ce_vec3f max;
} ce_aabb;

/* Infinite mathematical plane for frustum extraction and intersection routines (16B aligned) */
typedef struct CE_ALIGN_DECL(16) {
    CE_ALIGN_MEMBER(16) ce_vec3f normal;
    ce_f32 distance; /* Signed distance from the coordinate origin along the normal */
} CE_ALIGN_ATTR(16) ce_plane;

/**
 * @brief Spatial placement configuration within coordinate spaces.
 * Explicitly aligned to 16 bytes with explicit padding fields after `position` and `scale`:
 * 1. Eliminates 8 bytes of unnamed compiler tail padding (preventing memcmp/hash/serialization leaks).
 * 2. Places `rotation` (offset 0), `position` (offset 16), and `scale` (offset 32) on 16-byte
 *    SIMD boundaries so all three fields support aligned 128-bit SIMD loads (`_mm_load_ps`).
 */
typedef struct CE_ALIGN_DECL(16) {
    CE_ALIGN_MEMBER(16) ce_quat rotation; /* 16 bytes (offset 0)  */
    ce_vec3f position;                    /* 12 bytes (offset 16) */
    ce_f32   pad_pos;                     /*  4 bytes (offset 28) - explicit zeroed padding */
    ce_vec3f scale;                       /* 12 bytes (offset 32) */
    ce_f32   pad_scale;                   /*  4 bytes (offset 44) - explicit zeroed padding */
} CE_ALIGN_ATTR(16) ce_transform;

/* -------------------------------------------------------------------------- */
/* Graphics & Colors                                                          */
/* -------------------------------------------------------------------------- */

/* Floating point color layout aligned to 16 bytes for rapid GPU Constant Buffer uploads */
typedef struct CE_ALIGN_DECL(16) {
    CE_ALIGN_MEMBER(16) ce_f32 r, g, b, a;
} CE_ALIGN_ATTR(16) ce_colorf;

/* Packaged 32-bit RGBA color value (R in bits 0..7, G in 8..15, B in 16..23, A in 24..31). */
typedef ce_u32 ce_color32;

#define CE_COLOR32_PACK(r, g, b, a) \
    (((ce_u32)(ce_u8)(r))         | \
    (((ce_u32)(ce_u8)(g)) << 8)   | \
    (((ce_u32)(ce_u8)(b)) << 16)  | \
    (((ce_u32)(ce_u8)(a)) << 24))

#define CE_COLOR32_R(c) ((ce_u8)((c) & 0xFFu))
#define CE_COLOR32_G(c) ((ce_u8)(((c) >> 8) & 0xFFu))
#define CE_COLOR32_B(c) ((ce_u8)(((c) >> 16) & 0xFFu))
#define CE_COLOR32_A(c) ((ce_u8)(((c) >> 24) & 0xFFu))

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

/* Static-Initializer-Safe Constants & Runtime Compound Literals */
#define CE_INVALID_ENTITY_INIT   { 0xFFFFFFFFu, 0u }
#define CE_INVALID_ASSET_ID_INIT { 0ULL }
#define CE_INVALID_GUID_INIT     { 0ULL, 0ULL }

#if defined(__cplusplus)
    #define CE_INVALID_ENTITY   (ce_entity{ 0xFFFFFFFFu, 0u })
    #define CE_INVALID_ASSET_ID (ce_asset_id{ 0ULL })
    #define CE_INVALID_GUID     (ce_guid{ 0ULL, 0ULL })
#else
    #define CE_INVALID_ENTITY   ((ce_entity)CE_INVALID_ENTITY_INIT)
    #define CE_INVALID_ASSET_ID ((ce_asset_id)CE_INVALID_ASSET_ID_INIT)
    #define CE_INVALID_GUID     ((ce_guid)CE_INVALID_GUID_INIT)
#endif

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
/* Compile-Time Static Validation Checks (Size, Alignment & Padding)          */
/* -------------------------------------------------------------------------- */

CE_STATIC_ASSERT(sizeof(ce_i8)  == 1, "ce_i8 size invalid");
CE_STATIC_ASSERT(sizeof(ce_i16) == 2, "ce_i16 size invalid");
CE_STATIC_ASSERT(sizeof(ce_i32) == 4, "ce_i32 size invalid");
CE_STATIC_ASSERT(sizeof(ce_i64) == 8, "ce_i64 size invalid");
CE_STATIC_ASSERT(sizeof(ce_u8)  == 1, "ce_u8 size invalid");
CE_STATIC_ASSERT(sizeof(ce_u16) == 2, "ce_u16 size invalid");
CE_STATIC_ASSERT(sizeof(ce_u32) == 4, "ce_u32 size invalid");
CE_STATIC_ASSERT(sizeof(ce_u64) == 8, "ce_u64 size invalid");

CE_STATIC_ASSERT(sizeof(ce_b8)  == 1, "ce_b8 size invalid");
CE_STATIC_ASSERT(sizeof(ce_b32) == 4, "ce_b32 size invalid");

CE_STATIC_ASSERT(sizeof(ce_f16_bits) == 2, "ce_f16_bits size layout allocation invalid");
CE_STATIC_ASSERT(sizeof(ce_f32)      == 4, "ce_f32 size invalid");
CE_STATIC_ASSERT(sizeof(ce_f64)      == 8, "ce_f64 size invalid");

CE_STATIC_ASSERT(sizeof(ce_char16) == 2, "ce_char16 size invalid");
CE_STATIC_ASSERT(sizeof(ce_char32) == 4, "ce_char32 size invalid");

/* Verify both sizeof AND actual hardware alignment (CE_ALIGNOF) for SIMD types */
CE_STATIC_ASSERT(sizeof(ce_vec4f)     == 16 && CE_ALIGNOF(ce_vec4f)     == 16, "ce_vec4f SIMD alignment violation");
CE_STATIC_ASSERT(sizeof(ce_mat2)      == 16 && CE_ALIGNOF(ce_mat2)      == 16, "ce_mat2 SIMD alignment violation");
CE_STATIC_ASSERT(sizeof(ce_mat4)      == 64 && CE_ALIGNOF(ce_mat4)      == 16, "ce_mat4 SIMD alignment violation");
CE_STATIC_ASSERT(sizeof(ce_quat)      == 16 && CE_ALIGNOF(ce_quat)      == 16, "ce_quat SIMD alignment violation");
CE_STATIC_ASSERT(sizeof(ce_plane)     == 16 && CE_ALIGNOF(ce_plane)     == 16, "ce_plane SIMD alignment violation");
CE_STATIC_ASSERT(sizeof(ce_simd4f)    == 16 && CE_ALIGNOF(ce_simd4f)    == 16, "ce_simd4f platform alignment mismatch");
CE_STATIC_ASSERT(sizeof(ce_colorf)    == 16 && CE_ALIGNOF(ce_colorf)    == 16, "ce_colorf GPU boundary alignment violation");
CE_STATIC_ASSERT(sizeof(ce_transform) == 48 && CE_ALIGNOF(ce_transform) == 16, "ce_transform alignment/size violation");

/* Verify zero unnamed internal or tail padding in serialized/hashed structures */
CE_STATIC_ASSERT(offsetof(ce_transform, rotation) == 0,  "ce_transform.rotation offset mismatch");
CE_STATIC_ASSERT(offsetof(ce_transform, position) == 16, "ce_transform.position must be 16B aligned");
CE_STATIC_ASSERT(offsetof(ce_transform, scale)    == 32, "ce_transform.scale must be 16B aligned");
CE_STATIC_ASSERT(sizeof(ce_version) == 8, "ce_version structural memory leakage padding detected");
CE_STATIC_ASSERT(sizeof(ce_entity)  == 8, "ce_entity size mismatch");
CE_STATIC_ASSERT(sizeof(ce_guid)    == 16, "ce_guid size mismatch");

#ifdef __cplusplus
}
#endif

#endif /* CE_TYPES_H */
