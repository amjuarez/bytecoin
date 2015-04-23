// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "ITransaction.h"
#include <functional>
#include <cstring>

namespace CryptoNote {

inline bool operator==(const AccountAddress &_v1, const AccountAddress &_v2) {
  return memcmp(&_v1, &_v2, sizeof(AccountAddress)) == 0;
}

}

namespace std {

template<>
struct hash < CryptoNote::AccountAddress > {
  std::size_t operator()(const CryptoNote::AccountAddress& val) const {
    size_t spend = *(reinterpret_cast<const size_t*>(&val.spendPublicKey));
    size_t view = *(reinterpret_cast<const size_t*>(&val.viewPublicKey));
    return spend ^ view;
  }
};

template<>
struct hash < CryptoNote::PublicKey > {
  std::size_t operator()(const CryptoNote::PublicKey& val) const {
    return *reinterpret_cast<const size_t*>(&val);
  }
};

}
