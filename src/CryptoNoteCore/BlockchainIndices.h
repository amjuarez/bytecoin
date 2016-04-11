// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
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

#include <string>
#include <unordered_map>
#include <map>

#include "crypto/hash.h"
#include "CryptoNoteBasic.h"

namespace CryptoNote {

class ISerializer;

class PaymentIdIndex {
public:
  PaymentIdIndex() = default;

  bool add(const Transaction& transaction);
  bool remove(const Transaction& transaction);
  bool find(const Crypto::Hash& paymentId, std::vector<Crypto::Hash>& transactionHashes);
  void clear();

  void serialize(ISerializer& s);

  template<class Archive> 
  void serialize(Archive& archive, unsigned int version) {
    archive & index;
  }
private:
  std::unordered_multimap<Crypto::Hash, Crypto::Hash> index;
};

class TimestampBlocksIndex {
public:
  TimestampBlocksIndex() = default;

  bool add(uint64_t timestamp, const Crypto::Hash& hash);
  bool remove(uint64_t timestamp, const Crypto::Hash& hash);
  bool find(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t hashesNumberLimit, std::vector<Crypto::Hash>& hashes, uint32_t& hashesNumberWithinTimestamps);
  void clear();

  void serialize(ISerializer& s);

  template<class Archive> 
  void serialize(Archive& archive, unsigned int version) {
    archive & index;
  }
private:
  std::multimap<uint64_t, Crypto::Hash> index;
};

class TimestampTransactionsIndex {
public:
  TimestampTransactionsIndex() = default;

  bool add(uint64_t timestamp, const Crypto::Hash& hash);
  bool remove(uint64_t timestamp, const Crypto::Hash& hash);
  bool find(uint64_t timestampBegin, uint64_t timestampEnd, uint64_t hashesNumberLimit, std::vector<Crypto::Hash>& hashes, uint64_t& hashesNumberWithinTimestamps);
  void clear();

  void serialize(ISerializer& s);

  template<class Archive>
  void serialize(Archive& archive, unsigned int version) {
    archive & index;
  }
private:
  std::multimap<uint64_t, Crypto::Hash> index;
};

class GeneratedTransactionsIndex {
public:
  GeneratedTransactionsIndex();

  bool add(const Block& block);
  bool remove(const Block& block);
  bool find(uint32_t height, uint64_t& generatedTransactions);
  void clear();

  void serialize(ISerializer& s);

  template<class Archive> 
  void serialize(Archive& archive, unsigned int version) {
    archive & index;
    archive & lastGeneratedTxNumber;
  }
private:
  std::unordered_map<uint32_t, uint64_t> index;
  uint64_t lastGeneratedTxNumber;
};

class OrphanBlocksIndex {
public:
  OrphanBlocksIndex() = default;

  bool add(const Block& block);
  bool remove(const Block& block);
  bool find(uint32_t height, std::vector<Crypto::Hash>& blockHashes);
  void clear();
private:
  std::unordered_multimap<uint32_t, Crypto::Hash> index;
};

}
