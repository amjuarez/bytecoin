#pragma once

#include <cstddef>
#include <cstring>
#include <functional>

#define CRYPTO_MAKE_COMPARABLE(type) \
namespace crypto { \
  inline bool operator==(const type &_v1, const type &_v2) { \
    return std::memcmp(&_v1, &_v2, sizeof(type)) == 0; \
  } \
  inline bool operator!=(const type &_v1, const type &_v2) { \
    return std::memcmp(&_v1, &_v2, sizeof(type)) != 0; \
  } \
}

#define CRYPTO_MAKE_HASHABLE(type) \
CRYPTO_MAKE_COMPARABLE(type) \
namespace crypto { \
  static_assert(sizeof(std::size_t) <= sizeof(type), "Size of " #type " must be at least that of size_t"); \
  inline std::size_t hash_value(const type &_v) { \
    return reinterpret_cast<const std::size_t &>(_v); \
  } \
} \
namespace std { \
  template<> \
  struct hash<crypto::type> { \
    std::size_t operator()(const crypto::type &_v) const { \
      return reinterpret_cast<const std::size_t &>(_v); \
    } \
  }; \
}
