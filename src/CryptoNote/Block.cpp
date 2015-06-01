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

#include "Block.h"
#include "KeyInput.h"
#include "KeyOutput.h"
#include "MultisignatureInput.h"
#include "MultisignatureOutput.h"
#include "Transaction.h"

namespace CryptoNote {

Block::Block(
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
  std::vector<crypto::hash>&& branch) :
  majorVersion(majorVersion),
  minorVersion(minorVersion),
  timestamp(timestamp),
  previousBlockHash(previousBlockHash),
  baseTransaction(std::move(baseTransaction)),
  transactions(std::move(transactions)),
  parentMajorVersion(parentMajorVersion),
  parentMinorVersion(parentMinorVersion),
  nonce(nonce),
  parentPreviousBlockHash(parentPreviousBlockHash),
  parentBaseTransaction(std::move(parentBaseTransaction)),
  parentBaseTransactionBranch(std::move(parentBaseTransactionBranch)),
  parentTransactionCount(parentTransactionCount),
  branch(std::move(branch)) {
}

uint8_t Block::getMajorVersion() const {
  return majorVersion;
}

uint8_t Block::getMinorVersion() const {
  return minorVersion;
}

uint64_t Block::getTimestamp() const {
  return timestamp;
}

const crypto::hash& Block::getPreviousBlockHash() const {
  return previousBlockHash;
}

const BaseTransaction& Block::getBaseTransaction() const {
  return baseTransaction;
}

uint32_t Block::getTransactionCount() const {
  return static_cast<uint32_t>(transactions.size());
}

const Transaction& Block::getTransaction(uint32_t index) const {
  return transactions[index];
}

uint8_t Block::getParentMajorVersion() const {
  return parentMajorVersion;
}

uint8_t Block::getParentMinorVersion() const {
  return parentMinorVersion;
}

uint32_t Block::getNonce() const {
  return nonce;
}

const crypto::hash& Block::getParentPreviousBlockHash() const {
  return parentPreviousBlockHash;
}

const BaseTransaction& Block::getParentBaseTransaction() const {
  return parentBaseTransaction;
}

const std::vector<crypto::hash>& Block::getParentBaseTransactionBranch() const {
  return parentBaseTransactionBranch;
}

uint32_t Block::getParentTransactionCount() const {
  return parentTransactionCount;
}

const std::vector<crypto::hash>& Block::getBranch() const {
  return branch;
}

}
