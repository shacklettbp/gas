/*
 * Copyright 2021-2022 Brennan Shacklett and contributors
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */
#pragma once

#define GAS_STRINGIFY_HELPER(m) #m
#define GAS_STRINGIFY(m) GAS_STRINGIFY_HELPER(m)
#define GAS_MACRO_CONCAT_HELPER(x, y) x##y
#define GAS_MACRO_CONCAT(x, y) GAS_MACRO_CONCAT_HELPER(x, y)

#define GAS_LOC_APPEND(m) m ": " __FILE__ " @ " GAS_STRINGIFY(__LINE__)

#if defined(GAS_CXX_CLANG) or defined(GAS_CXX_GCC)
#define GAS_COMPILER_FUNCTION_NAME __PRETTY_FUNCTION__
#elif defined(GAS_CXX_MSVC)
#define GAS_COMPILER_FUNCTION_NAME __FUNCSIG__
#endif

#ifdef GAS_ARCH_X64
#define GAS_CACHE_LINE (64)
#elif defined(GAS_ARCH_ARM) && defined(GAS_OS_MACOS)
#define GAS_CACHE_LINE (128)
#else
#define GAS_CACHE_LINE (64)
#endif

#if defined(GAS_CXX_MSVC)

#define GAS_NO_INLINE __declspec(noinline)
#if defined(GAS_CXX_CLANG_CL)
#define GAS_ALWAYS_INLINE __attribute__((always_inline))
#else
#define GAS_ALWAYS_INLINE [[msvc::forceinline]]
#endif

#elif defined(GAS_CXX_CLANG) || defined(GAS_CXX_GCC)

#define GAS_ALWAYS_INLINE __attribute__((always_inline))
#define GAS_NO_INLINE __attribute__((noinline))

#endif

#if defined(GAS_OS_WINDOWS)
#define GAS_IMPORT __declspec(dllimport)
#define GAS_EXPORT __declspec(dllexport)
#else
#define GAS_IMPORT __attribute__ ((visibility ("default")))
#define GAS_EXPORT __attribute__ ((visibility ("default")))
#endif

#if defined(GAS_CXX_MSVC)
#define GAS_UNREACHABLE() __assume(0)
#else
#define GAS_UNREACHABLE() __builtin_unreachable()
#endif

#if defined(GAS_CXX_CLANG) || defined(GAS_CXX_CLANG_CL)
#define GAS_LFBOUND [[clang::lifetimebound]]
#elif defined(GAS_CXX_MSVC)
#define GAS_LFBOUND [[msvc::lifetimebound]]
#else
#define GAS_LFBOUND
#endif

#define GAS_UNIMPLEMENTED() \
    static_assert(false, "Unimplemented")

#if defined(GAS_CXX_CLANG)
#define GAS_UNROLL _Pragma("unroll")
#else
#define GAS_UNROLL
#endif
