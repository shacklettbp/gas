namespace mwrt {

template <typename T>
ArrayQueue<T>::ArrayQueue(T *data, u32 capacity)
    : data_(data),
      capacity_(capacity),
      head_(0),
      tail_(0)
{}

template <typename T>
void ArrayQueue<T>::add(T t)
{
    data_[tail_] = t;
    tail_ = increment(tail_);
}

template <typename T>
T ArrayQueue<T>::remove()
{
    T t = data_[head_];
    head_ = increment(head_);
    return t;
}

template <typename T>
u32 ArrayQueue<T>::capacity() const
{
    return capacity_;
}

template <typename T>
bool ArrayQueue<T>::isEmpty() const
{
    return head_ == tail_;
}

template <typename T>
void ArrayQueue<T>::clear()
{
    head_ = 0;
    tail_ = 0;
}

template <typename T>
u32 ArrayQueue<T>::increment(u32 i)
{
    if (i == capacity_ - 1) {
        return 0;
    }

    return i + 1;
}

template <typename Fn>
[[nodiscard]] auto defer(Fn &&fn)
{
  struct Defer : Fn {
    Defer(Fn &&fn) : Fn(std::forward<Fn>(fn)) {}
    Defer(Defer &) = delete;
    Defer(Defer &&) = delete;
    ~Defer() { (*this)(); }
  };

  return Defer { std::forward<Fn>(fn) };
}

template <typename T>
constexpr inline T divideRoundUp(T a, T b)
{
    static_assert(std::is_integral_v<T>);

    return (a + (b - 1)) / b;
}

template <typename T>
constexpr inline T roundUp(T offset, T alignment)
{
    return divideRoundUp(offset, alignment) * alignment;
}

// alignment must be power of 2
constexpr inline u64 roundToAlignment(u64 offset, u64 alignment)
{
#ifdef PROA_CXX_MSVC
#pragma warning(push)
#pragma warning(disable : 4146)
#endif
    return (offset + alignment - 1) & -alignment;
#ifdef PROA_CXX_MSVC
#pragma warning(pop)
#endif
}

constexpr inline i64 roundToAlignment(i64 offset, i64 alignment)
{
    return (i64)roundToAlignment((u64)offset, (u64)alignment);
}

constexpr inline u32 roundToAlignment(u32 offset, u32 alignment)
{
#ifdef PROA_CXX_MSVC
#pragma warning(push)
#pragma warning(disable : 4146)
#endif
    return (offset + alignment - 1) & -alignment;
#ifdef PROA_CXX_MSVC
#pragma warning(pop)
#endif
}

constexpr inline i32 roundToAlignment(i32 offset, i32 alignment)
{
    return (i32)roundToAlignment((u64)offset, (u64)alignment);
}

inline uintptr_t alignPtrOffset(void *ptr, uintptr_t alignment)
{
    uintptr_t base = (uintptr_t)ptr;
    static_assert(sizeof(uintptr_t) == sizeof(u64));
    uintptr_t aligned = roundToAlignment((u64)base, (u64)alignment);
    return aligned - base;
}

inline void *alignPtr(void *ptr, uintptr_t alignment)
{
    return (char *)ptr + alignPtrOffset(ptr, alignment);
}

constexpr inline bool isPower2(u64 v)
{
    return (v & (v - 1)) == 0;
}

constexpr inline bool isPower2(u32 v)
{
    return (v & (v - 1)) == 0;
}

constexpr inline u32 u32NextPow2(u32 v)
{
    return v == 1 ? 1 : (1u << (32u - __builtin_clz((int32_t)v - 1)));
}

constexpr inline u64 u64NextPow2(u64 v)
{
#if __cplusplus >= 202002L
    int clz;
#else
    int clz = 0;
#endif
    if constexpr (std::is_same_v<int64_t, long>) {
        clz = __builtin_clzl((long)v - 1);
    } else if constexpr (std::is_same_v<int64_t, long long>) {
        clz = __builtin_clzll((long long)v - 1);
    }

    return v == 1 ? 1 : (1u << (64u - clz));
}

constexpr inline u32 u32Log2(u32 v)
{
    static_assert(std::is_same_v<int32_t, int>);

    return sizeof(unsigned int) * 8 - __builtin_clz((int)v) - 1;
}

constexpr inline u64 u64Log2(u64 v)
{
    return sizeof(unsigned long long) * 8 - __builtin_clzll((long long)v) - 1;
}

// https://github.com/skeeto/hash-prospector
constexpr inline u32 u32Hash(u32 x)
{
    x ^= x >> 16u;
    x *= 0x7feb352du;
    x ^= x >> 15u;
    x *= 0x846ca68bu;
    x ^= x >> 16u;
    return x;
}

constexpr inline u32 u32mulhi(u32 a, u32 b)
{
    u64 m = u64(a) * u64(b);
    return u32(m >> 32);
}

inline i64 computeBufferOffsets(const Span<const i64> chunk_sizes,
                                Span<i64> out_offsets,
                                i64 pow2_alignment)
{
    i64 num_total_bytes = chunk_sizes[0];

    for (i64 i = 1; i < chunk_sizes.size(); i++) {
        i64 cur_offset = roundToAlignment(num_total_bytes, pow2_alignment);
        out_offsets[i - 1] = cur_offset;

        num_total_bytes = cur_offset + chunk_sizes[i];
    }

    return roundToAlignment(num_total_bytes, pow2_alignment);
}

template <typename T>
inline void copyN(std::type_identity_t<T> *dst,
                  const std::type_identity_t<T> *src,
                  i64 num_elems)
{
    memcpy(dst, src, sizeof(T) * num_elems);
}

template <typename T>
inline void zeroN(std::type_identity_t<T> *ptr, i64 num_elems)
{
    memset(ptr, 0, num_elems * sizeof(T));
}

template <typename T>
inline void fillN(std::type_identity_t<T> *ptr, T v, i64 num_elems)
{
    for (i64 i = 0 ; i < num_elems; i++) {
        ptr[i] = v;
    }
}

}
