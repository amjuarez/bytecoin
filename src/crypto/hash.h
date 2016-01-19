// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stddef.h>

#include <CryptoTypes.h>
#include "generic-ops.h"

namespace Crypto {

  extern "C" {
#include "hash-ops.h"
  }

  /*
    Cryptonight hash functions
  */

  inline void cn_fast_hash(const void *data, size_t length, Hash &hash) {
    cn_fast_hash(data, length, reinterpret_cast<char *>(&hash));
  }

  inline Hash cn_fast_hash(const void *data, size_t length) {
    Hash h;
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
    friend inline void cn_slow_hash(cn_context &, const void *, size_t, Hash &);
  };

  inline void cn_slow_hash(cn_context &context, const void *data, size_t length, Hash &hash) {
    (*cn_slow_hash_f)(context.data, data, length, reinterpret_cast<void *>(&hash));
  }

  inline void tree_hash(const Hash *hashes, size_t count, Hash &root_hash) {
    tree_hash(reinterpret_cast<const char (*)[HASH_SIZE]>(hashes), count, reinterpret_cast<char *>(&root_hash));
  }

  inline void tree_branch(const Hash *hashes, size_t count, Hash *branch) {
    tree_branch(reinterpret_cast<const char (*)[HASH_SIZE]>(hashes), count, reinterpret_cast<char (*)[HASH_SIZE]>(branch));
  }

  inline void tree_hash_from_branch(const Hash *branch, size_t depth, const Hash &leaf, const void *path, Hash &root_hash) {
    tree_hash_from_branch(reinterpret_cast<const char (*)[HASH_SIZE]>(branch), depth, reinterpret_cast<const char *>(&leaf), path, reinterpret_cast<char *>(&root_hash));
  }

}

CRYPTO_MAKE_HASHABLE(Hash)
