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

#include <stdint.h>
#include <stddef.h>

#define CHACHA_KEY_SIZE 32
#define CHACHA_IV_SIZE 8

#if defined(__cplusplus)
#include <memory.h>

#include "hash.h"

namespace crypto {
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

  inline void chacha8(const void* data, std::size_t length, const chacha_key& key, const chacha_iv& iv, char* cipher) {
    chacha(4, data, length, reinterpret_cast<const uint8_t*>(&key), reinterpret_cast<const uint8_t*>(&iv), cipher);
  }

  inline void generate_chacha8_key(crypto::cn_context &context, std::string password, chacha_key& key) {
    static_assert(sizeof(chacha_key) <= sizeof(hash), "Size of hash must be at least that of chacha_key");
    crypto::hash pwd_hash;
    crypto::cn_slow_hash(context, password.data(), password.size(), pwd_hash);
    memcpy(&key, &pwd_hash, sizeof(key));
    memset(&pwd_hash, 0, sizeof(pwd_hash));
  }
}

#endif
