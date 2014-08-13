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

#include "crypto/crypto.h"
#include "cryptonote_core/cryptonote_basic.h"

class test_cn_slow_hash
{
public:
  static const size_t loop_count = 10;

#pragma pack(push, 1)
  struct data_t
  {
    char data[13];
  };
#pragma pack(pop)

  static_assert(13 == sizeof(data_t), "Invalid structure size");

  bool init()
  {
    if (!epee::string_tools::hex_to_pod("63617665617420656d70746f72", m_data))
      return false;

    if (!epee::string_tools::hex_to_pod("bbec2cacf69866a8e740380fe7b818fc78f8571221742d729d9d02d7f8989b87", m_expected_hash))
      return false;

    return true;
  }

  bool test()
  {
    crypto::hash hash;
    crypto::cn_slow_hash(m_context, &m_data, sizeof(m_data), hash);
    return hash == m_expected_hash;
  }

private:
  data_t m_data;
  crypto::hash m_expected_hash;
  crypto::cn_context m_context;
};
