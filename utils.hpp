/*
 * Copyright 2021-2022 Brennan Shacklett and contributors
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */
#pragma once

#include "macros.hpp"
#include "namespace.hpp"
#include "span.hpp"

#ifdef GAS_CXX_MSVC
#include <bit>
#endif

#include <type_traits>

namespace gas {

template <typename T>
class ArrayQueue {
public:
    ArrayQueue(T *data, u32 capacity);

    void add(T t);
    T remove();

    u32 capacity() const;
    bool isEmpty() const;
    void clear();

private:
    u32 increment(u32 i);

    T *data_;
    u32 capacity_;
    u32 head_;
    u32 tail_;
};

template <typename Fn>
[[nodiscard]] auto defer(Fn &&fn);
#define GAS_DEFER(fn) \
  auto GAS_MACRO_CONCAT(_gas_defer_, __COUNTER__) = ::gas::defer([&]() { fn; })

template <typename T>
constexpr inline T divideRoundUp(T a, T b);

template <typename T>
constexpr inline T roundUp(T offset, T alignment);

// alignment must be power of 2
constexpr inline u64 roundToAlignment(u64 offset, u64 alignment);
constexpr inline i64 roundToAlignment(i64 offset, i64 alignment);
constexpr inline u32 roundToAlignment(u32 offset, u32 alignment);
constexpr inline i32 roundToAlignment(i32 offset, i32 alignment);

inline uintptr_t alignPtrOffset(void *ptr, uintptr_t alignment);
inline void * alignPtr(void *ptr, uintptr_t alignment);

constexpr inline bool isPower2(u64 v);
constexpr inline bool isPower2(u32 v);

constexpr inline u32 u32NextPow2(u32 v);
constexpr inline u64 u64NextPow2(u64 v);

constexpr inline u32 u32Log2(u32 v);
constexpr inline u64 u64Log2(u64 v);

constexpr inline u32 u32Hash(u32 x);

constexpr inline u32 u32mulhi(u32 a, u32 b);

i32 u32NumDigits(u32 x);

inline i64 computeBufferOffsets(const Span<i64> chunk_sizes,
                                Span<i64> out_offsets,
                                i64 pow2_alignment);

template <typename T>
inline void copyN(std::type_identity_t<T> *dst,
                  const std::type_identity_t<T> *src,
                  i64 num_elems);

template <typename T>
inline void zeroN(std::type_identity_t<T> *ptr, i64 num_elems);

template <typename T>
inline void fillN(std::type_identity_t<T> *ptr, T v, i64 num_elems);

}

#if defined(GAS_IS_GPU)

inline int __builtin_clz(unsigned v)
{
    return __clz((int)v);
}

inline int __builtin_clzl(unsigned long v)
{
    return __clzll((long int)v);
}

inline int __builtin_clzll(unsigned long long v)
{
    return __clzll((long long int)v);
}

inline int __builtin_ctz(unsigned v)
{
  return __clz((int)__brev(v));
}

inline int __builtin_ctzl(unsigned long v)
{
  return __clz((long int)__brev(v));
}

inline int __builtin_ctzl(unsigned long long v)
{
  return __clz((long long int)__brev(v));
}

namespace std {

inline int countl_zero(int v)
{
  return __clz(v);
}

inline int countl_zero(long int v)
{
  return __clzll(v);
}

inline int countl_zero(long long int v)
{
  return __clzll(v);
}

inline int countl_zero(unsigned v)
{
  return __clz((int)v);
}

inline int countl_zero(unsigned long v)
{
  return __clzll((long)v);
}

inline int countl_zero(unsigned long long v)
{
  return __clzll((long long)v);
}

}

#elif defined(GAS_CXX_MSVC)

GAS_ALWAYS_INLINE constexpr inline int __builtin_clz(int v)
{
    using U = std::make_unsigned_t<int>;
    return sizeof(U) * 8 - std::bit_width(U(v));
}

GAS_ALWAYS_INLINE constexpr inline int __builtin_clzl(long int v)
{
    using U = std::make_unsigned_t<long int>;
    return sizeof(U) * 8 - std::bit_width(U(v));
}

GAS_ALWAYS_INLINE constexpr inline int __builtin_clzll(long long int v)
{
    using U = std::make_unsigned_t<long long int>;
    return sizeof(U) * 8 - std::bit_width(U(v));
}

#endif

#include "utils.inl"
