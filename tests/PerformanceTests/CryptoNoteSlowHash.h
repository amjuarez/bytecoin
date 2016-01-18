// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "Common/StringTools.h"
#include "crypto/crypto.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"

class test_cn_slow_hash {
public:
  static const size_t loop_count = 10;

#pragma pack(push, 1)
  struct data_t {
    char data[13];
  };
#pragma pack(pop)

  static_assert(13 == sizeof(data_t), "Invalid structure size");

  bool init() {
    size_t size;
    if (!Common::fromHex("63617665617420656d70746f72", &m_data, sizeof(m_data), size) || size != sizeof(m_data)) {
      return false;
    }

    if (!Common::fromHex("bbec2cacf69866a8e740380fe7b818fc78f8571221742d729d9d02d7f8989b87", &m_expected_hash, sizeof(m_expected_hash), size) || size != sizeof(m_expected_hash)) {
      return false;
    }

    return true;
  }

  bool test() {
    Crypto::Hash hash;
    Crypto::cn_slow_hash(m_context, &m_data, sizeof(m_data), hash);
    return hash == m_expected_hash;
  }

private:
  data_t m_data;
  Crypto::Hash m_expected_hash;
  Crypto::cn_context m_context;
};
