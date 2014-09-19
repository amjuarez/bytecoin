// Copyright (c) 2012-2014, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <stddef.h>

#include "common/pod-class.h"
#include "generic-ops.h"

namespace crypto {

  extern "C" {
#include "hash-ops.h"
  }

#pragma pack(push, 1)
  POD_CLASS hash {
    char data[HASH_SIZE];
  };
#pragma pack(pop)

  static_assert(sizeof(hash) == HASH_SIZE, "Invalid structure size");

  /*
    Cryptonight hash functions
  */

  inline void cn_fast_hash(const void *data, std::size_t length, hash &hash) {
    cn_fast_hash(data, length, reinterpret_cast<char *>(&hash));
  }

  inline hash cn_fast_hash(const void *data, std::size_t length) {
    hash h;
    cn_fast_hash(data, length, reinterpret_cast<char *>(&h));
    return h;
  }

  class cn_context {
  public:

    cn_context();
    ~cn_context();
#if !defined(_MSC_VER) || _MSC_VER >= 1800
    cn_context(const cn_context &) = delete;
    void operator=(const cn_context &) = delete;
#endif

  private:

    void *data;
    friend inline void cn_slow_hash(cn_context &, const void *, std::size_t, hash &);
  };

  inline void cn_slow_hash(cn_context &context, const void *data, std::size_t length, hash &hash) {
    (*cn_slow_hash_f)(context.data, data, length, reinterpret_cast<void *>(&hash));
  }

  inline void tree_hash(const hash *hashes, std::size_t count, hash &root_hash) {
    tree_hash(reinterpret_cast<const char (*)[HASH_SIZE]>(hashes), count, reinterpret_cast<char *>(&root_hash));
  }

  inline void tree_branch(const hash *hashes, std::size_t count, hash *branch) {
    tree_branch(reinterpret_cast<const char (*)[HASH_SIZE]>(hashes), count, reinterpret_cast<char (*)[HASH_SIZE]>(branch));
  }

  inline void tree_hash_from_branch(const hash *branch, std::size_t depth, const hash &leaf, const void *path, hash &root_hash) {
    tree_hash_from_branch(reinterpret_cast<const char (*)[HASH_SIZE]>(branch), depth, reinterpret_cast<const char *>(&leaf), path, reinterpret_cast<char *>(&root_hash));
  }

}

CRYPTO_MAKE_HASHABLE(hash)
