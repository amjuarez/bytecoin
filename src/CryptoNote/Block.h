// Copyright (c) 2012-2015, The CryptoNote developers, The Bytecoin developers
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

#include "BaseTransaction.h"

namespace CryptoNote {

class Transaction;

class Block {
public:
  Block(
    uint8_t majorVersion,
    uint8_t minorVersion,
    uint64_t timestamp,
    const crypto::hash& previousBlockHash,
    BaseTransaction&& baseTransaction,
    std::vector<Transaction>&& transactions,
    uint8_t parentMajorVersion,
    uint8_t parentMinorVersion,
    uint32_t nonce,
    const crypto::hash& parentPreviousBlockHash,
    BaseTransaction&& parentBaseTransaction,
    std::vector<crypto::hash>&& parentBaseTransactionBranch,
    uint32_t parentTransactionCount,
    std::vector<crypto::hash>&& branch);
  Block(const Block& other) = delete;
  Block& operator=(const Block& other) = delete;
  uint8_t getMajorVersion() const;
  uint8_t getMinorVersion() const;
  uint64_t getTimestamp() const;
  const crypto::hash& getPreviousBlockHash() const;
  const BaseTransaction& getBaseTransaction() const;
  uint32_t getTransactionCount() const;
  const Transaction& getTransaction(uint32_t index) const;
  uint8_t getParentMajorVersion() const;
  uint8_t getParentMinorVersion() const;
  uint32_t getNonce() const;
  const crypto::hash& getParentPreviousBlockHash() const;
  const BaseTransaction& getParentBaseTransaction() const;
  const std::vector<crypto::hash>& getParentBaseTransactionBranch() const;
  uint32_t getParentTransactionCount() const;
  const std::vector<crypto::hash>& getBranch() const;

private:
  uint8_t majorVersion;
  uint8_t minorVersion;
  uint64_t timestamp;
  crypto::hash previousBlockHash;
  BaseTransaction baseTransaction;
  std::vector<Transaction> transactions;
  uint8_t parentMajorVersion;
  uint8_t parentMinorVersion;
  uint32_t nonce;
  crypto::hash parentPreviousBlockHash;
  BaseTransaction parentBaseTransaction;
  std::vector<crypto::hash> parentBaseTransactionBranch;
  uint32_t parentTransactionCount;
  std::vector<crypto::hash> branch;
};

}
