// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <vector>
#include <ostream>
#include <istream>

#include "crypto/hash.h"
#include "crypto/chacha8.h"

namespace cryptonote {
class account_base;
class ISerializer;
}

namespace CryptoNote {

class WalletUserTransactionsCache;

class WalletSerializer {
public:
  WalletSerializer(cryptonote::account_base& account, WalletUserTransactionsCache& transactionsCache);

  void serialize(std::ostream& stream, const std::string& password, bool saveDetailed, const std::string& cache);
  void deserialize(std::istream& stream, const std::string& password, std::string& cache);

private:
  void saveKeys(cryptonote::ISerializer& serializer);
  void loadKeys(cryptonote::ISerializer& serializer);

  crypto::chacha8_iv encrypt(const std::string& plain, const std::string& password, std::string& cipher);
  void decrypt(const std::string& cipher, std::string& plain, crypto::chacha8_iv iv, const std::string& password);

  cryptonote::account_base& account;
  WalletUserTransactionsCache& transactionsCache;
  const uint32_t walletSerializationVersion;
};

} //namespace CryptoNote
