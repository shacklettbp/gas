#pragma once

#include "namespace.hpp"

#include <madrona/utils.hpp>

namespace gas {

// Note these aren't any kind of standardized UUIDs. Just 128 bit random values
struct UUID {
  u64 id[2];

  static inline constexpr UUID parse(const char *str, size_t size);
  static inline constexpr UUID randomFromSeedString(
    const char *str, size_t size);

  inline u64 operator[](size_t idx) const { return id[idx]; }
  inline u64 & operator[](size_t idx) { return id[idx]; }

  constexpr inline friend bool operator==(UUID a, UUID b)
  {
    return a.id[0] == b.id[0] && a.id[1] == b.id[1];
  }
};

inline constexpr UUID operator "" _uuid(const char *str, size_t size)
{
  return UUID::parse(str, size);
}

inline constexpr UUID operator "" _to_uuid(const char *str, size_t size)
{
  return UUID::randomFromSeedString(str, size);
}

constexpr UUID UUID::parse(const char *str, size_t size)
{
  if (size != 36) {
    return {{ 0, 0, }};
  }

  if (str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-') {
    return {{ 0, 0, }};
  }

  bool error = false;
  auto hex = [&error](const char *str, size_t offset) -> u64
  {
    // Upper case
    const char a = str[offset] | 0x20;

    if (a >= '0' && a <= '9') {
      return a - '0';
    } else if (a >= 'a' && a <= 'f') {
      return 10 + a - 'a';
    } else {
      error = true;
      return 0;
    }
  };

  u64 a =
    hex(str,  0) << 60 |
    hex(str,  1) << 56 |
    hex(str,  2) << 52 |
    hex(str,  3) << 48 |
    hex(str,  4) << 44 |
    hex(str,  5) << 40 |
    hex(str,  6) << 36 |
    hex(str,  7) << 32 |
    hex(str,  9) << 28 |
    hex(str, 10) << 24 |
    hex(str, 11) << 20 |
    hex(str, 12) << 16 |
    hex(str, 14) << 12 |
    hex(str, 15) << 8  |
    hex(str, 16) << 4  |
    hex(str, 17);

  u64 b =
    hex(str, 19) << 60 |
    hex(str, 20) << 56 |
    hex(str, 21) << 52 |
    hex(str, 22) << 48 |
    hex(str, 24) << 44 |
    hex(str, 25) << 40 |
    hex(str, 26) << 36 |
    hex(str, 27) << 32 |
    hex(str, 28) << 28 |
    hex(str, 29) << 24 |
    hex(str, 30) << 20 |
    hex(str, 31) << 16 |
    hex(str, 32) << 12 |
    hex(str, 33) << 8  |
    hex(str, 34) << 4  |
    hex(str, 35);

  if (error) {
    return {{ 0, 0 }};
  }

  return {{ b, a }};
}

constexpr UUID UUID::randomFromSeedString(const char *str, size_t size)
{
  if (size == 0) {
    return {{ 0, 0 }};
  }

  // 128 bit murmurhash 3

  u64 h1 = 0;
  u64 h2 = 0;

  const u64 c1 = 0x87c37b91114253d5;
  const u64 c2 = 0x4cf5ad432745937f;

  auto rot = [](u64 x, u64 r) {
    return (x << r) | (x >> (64 - r));
  };

  u64 rounded_size = utils::roundUp((u64)size, (u64)16);
  for (u64 i = 0; i < rounded_size; i += 16) {
    uint64_t k1 = 0;
    for (size_t j = 0; j < 8; j++) {
      size_t idx = i + j;
      char c;
      if (idx < size) {
        c = str[idx];
      } else {
        c = 0;
      }

      k1 |= c << j;
    }

    uint64_t k2 = 0;

    for (size_t j = 0; j < 8; j++) {
      size_t idx = i + 8 + j;
      char c;
      if (idx < size) {
        c = str[idx];
      } else {
        c = 0;
      }

      k2 |= c << j;
    }

    k1 *= c1;
    k1 = rot(k1, 31);
    k1 *= c2;
    h1 ^= k1;

    h1 = rot(h1, 27);
    h1 += h2;
    h1 = h1 * 5 + 0x52dce729;

    k2 *= c2;
    k2 = rot(k2, 33);
    k2 *= c1;
    h2 ^= k2;

    h2 = rot(h2, 31);
    h2 += h1;
    h2 = h2 * 5 + 0x38495ab5;
  }

  return UUID { h1, h2 };
}

}
