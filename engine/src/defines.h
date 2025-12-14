#pragma once

// Unsigned int types.
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

// Signed int types.
typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef signed long long i64;

// Floating point types
typedef float f32;
typedef double f64;

// Boolean types
typedef int b32;
typedef _Bool b8;

#define INT8_MAX         127i8
#define INT16_MAX        32767i16
#define INT32_MAX        2147483647i32
#define INT64_MAX        9223372036854775807i64
#define UINT8_MAX        0xffui8
#define UINT16_MAX       0xffffui16
#define UINT32_MAX       0xffffffffui32
#define UINT64_MAX       0xffffffffffffffffui64

// Properly define static assertions.
#if defined(__clang__) || defined(__gcc__)
#define STATIC_ASSERT _Static_assert
#else
#define STATIC_ASSERT static_assert
#endif

// Ensure all types are of the correct size.
STATIC_ASSERT(sizeof(u8) == 1, "Expected u8 to be 1 byte.");
STATIC_ASSERT(sizeof(u16) == 2, "Expected u16 to be 2 bytes.");
STATIC_ASSERT(sizeof(u32) == 4, "Expected u32 to be 4 bytes.");
STATIC_ASSERT(sizeof(u64) == 8, "Expected u64 to be 8 bytes.");

STATIC_ASSERT(sizeof(i8) == 1, "Expected i8 to be 1 byte.");
STATIC_ASSERT(sizeof(i16) == 2, "Expected i16 to be 2 bytes.");
STATIC_ASSERT(sizeof(i32) == 4, "Expected i32 to be 4 bytes.");
STATIC_ASSERT(sizeof(i64) == 8, "Expected i64 to be 8 bytes.");

STATIC_ASSERT(sizeof(f32) == 4, "Expected f32 to be 4 bytes.");
STATIC_ASSERT(sizeof(f64) == 8, "Expected f64 to be 8 bytes.");

#define TRUE 1
#define FALSE 0
#define NULL ((void *)0)

// Always define bxdebug_break in case it is ever needed outside assertions (i.e fatal log errors)
// Try via __has_builtin first.
#if defined(__has_builtin) && !defined(__ibmxl__)
#    if __has_builtin(__builtin_debugtrap)
#        define bxdebug_break() __builtin_debugtrap()
#    elif __has_builtin(__debugbreak)
#        define bxdebug_break() __debugbreak()
#    endif
#endif

// If not setup, try the old way.
#if !defined(bxdebug_break)
#    if defined(__clang__) || defined(__gcc__)
#        define bxdebug_break() __builtin_trap()
#    elif defined(_MSC_VER)
#        include <intrin.h>
#        define bxdebug_break() __debugbreak()
#    else
// Fall back to x86/x86_64
#        define bxdebug_break() asm { int 3 }
#    endif
#endif

// Compiler-specific stuff
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#   define BX_NORETURN _Noreturn
#   elif defined(__GNUC__)
#       define BX_NORETURN __attribute__((__noreturn__))
#   else
#       define BX_NORETURN
#endif

#if !(defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201102L)) && !defined(_Thread_local)
#   if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__SUNPRO_CC) || defined(__IBMCPP__)
#       define _Thread_local __thread
#   else
#       define _Thread_local __declspec(thread)
#   endif
#elif defined(__GNUC__) && defined(__GNUC_MINOR__) && (((__GNUC__ << 8) | __GNUC_MINOR__) < ((4 << 8) | 9))
#   define _Thread_local __thread
#endif

// Platform detection
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) 
#define BX_PLATFORM_WINDOWS 1
#ifndef _WIN64
#error "64-bit is required on Windows!"
#endif
#elif defined(__linux__) || defined(__gnu_linux__)
// Linux OS
#define BX_PLATFORM_LINUX 1
#if defined(__ANDROID__)
#define BX_PLATFORM_ANDROID 1
#endif
#elif defined(__unix__)
// Catch anything not caught by the above.
#define BX_PLATFORM_UNIX 1
#elif defined(_POSIX_VERSION)
// Posix
#define BX_PLATFORM_POSIX 1
#elif __APPLE__
// Apple platforms
#define BX_PLATFORM_APPLE 1
#include <TargetConditionals.h>
#if TARGET_IPHONE_SIMULATOR
// iOS Simulator
#define BX_PLATFORM_IOS 1
#define BX_PLATFORM_IOS_SIMULATOR 1
#elif TARGET_OS_IPHONE
#define BX_PLATFORM_IOS 1
// iOS device
#elif TARGET_OS_MAC
// Other kinds of Mac OS
#else
#error "Unknown Apple platform"
#endif
#else
#error "Unknown platform!"
#endif

#define BX_OFFSETOF(s, m) (&(((s*)0)->m))

#define BX_CLAMP(value, min, max) ((value <= min) ? min : (value >= max) ? max : value)

#define BX_MIN(x, y) (x < y ? x : y)
#define BX_MAX(x, y) (x > y ? x : y)

#define BX_ARRAYSIZE(arr) (sizeof(arr) / sizeof(*arr))

static u64 alignment(u64 v, u64 align) {
    return (v + (align - 1)) & ~(align - 1);
}

#include "logger.h"

#include "platform/platform.h"
#include "utils/event.h"
#include "utils/math_types.h"