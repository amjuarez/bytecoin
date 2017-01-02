// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2017 XDN-project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdint.h>
#include <stddef.h>

#define CHACHA_KEY_SIZE 32
#define CHACHA_IV_SIZE 8

#if defined(__cplusplus)
#include <memory.h>
#include <string>

#include "hash.h"

namespace Crypto {
  extern "C" {
#endif
    void chacha(size_t doubleRounds, const void* data, size_t length, const uint8_t* key, const uint8_t* iv, char* cipher);
#if defined(__cplusplus)
  }

#pragma pack(push, 1)
  struct chacha_key {
    uint8_t data[CHACHA_KEY_SIZE];

    ~chacha_key()
    {
      memset(data, 0, sizeof(data));
    }
  };

  // MS VC 2012 doesn't interpret `class chacha_iv` as POD in spite of [9.0.10], so it is a struct
  struct chacha_iv {
    uint8_t data[CHACHA_IV_SIZE];
  };
#pragma pack(pop)

  static_assert(sizeof(chacha_key) == CHACHA_KEY_SIZE && sizeof(chacha_iv) == CHACHA_IV_SIZE, "Invalid structure size");

  inline void chacha8(const void* data, size_t length, const chacha_key& key, const chacha_iv& iv, char* cipher) {
    chacha(4, data, length, reinterpret_cast<const uint8_t*>(&key), reinterpret_cast<const uint8_t*>(&iv), cipher);
  }

  inline void generate_chacha8_key(Crypto::cn_context &context, const std::string& password, chacha_key& key) {
    static_assert(sizeof(chacha_key) <= sizeof(Hash), "Size of hash must be at least that of chacha_key");
    Hash pwd_hash;
    cn_slow_hash(context, password.data(), password.size(), pwd_hash);
    memcpy(&key, &pwd_hash, sizeof(key));
    memset(&pwd_hash, 0, sizeof(pwd_hash));
  }
}

#endif
