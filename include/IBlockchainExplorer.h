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

#include <vector>
#include <array>

#include "BlockchainExplorerData.h"

namespace CryptoNote {

class IBlockchainObserver {
public:
  virtual ~IBlockchainObserver() {}

  virtual void blockchainUpdated(const std::vector<BlockDetails>& newBlocks, const std::vector<BlockDetails>& orphanedBlocks) {}
  virtual void poolUpdated(const std::vector<TransactionDetails>& newTransactions, const std::vector<std::pair<std::array<uint8_t, 32>, TransactionRemoveReason>>& removedTransactions) {}

  virtual void blockchainSynchronized(const BlockDetails& topBlock) {}
};

class IBlockchainExplorer {
public:
  virtual ~IBlockchainExplorer() {};

  virtual bool addObserver(IBlockchainObserver* observer) = 0;
  virtual bool removeObserver(IBlockchainObserver* observer) = 0;

  virtual void init() = 0;
  virtual void shutdown() = 0;

  virtual bool getBlocks(const std::vector<uint64_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks) = 0;
  virtual bool getBlocks(const std::vector<std::array<uint8_t, 32>>& blockHashes, std::vector<BlockDetails>& blocks) = 0;

  virtual bool getBlockchainTop(BlockDetails& topBlock) = 0;

  virtual bool getTransactions(const std::vector<std::array<uint8_t, 32>>& transactionHashes, std::vector<TransactionDetails>& transactions) = 0;
  virtual bool getPoolState(const std::vector<std::array<uint8_t, 32>>& knownPoolTransactionHashes, std::array<uint8_t, 32> knownBlockchainTop, bool& isBlockchainActual, std::vector<TransactionDetails>& newTransactions, std::vector<std::array<uint8_t, 32>>& removedTransactions) = 0;

  virtual uint64_t getRewardBlocksWindow() = 0;
  virtual uint64_t getFullRewardMaxBlockSize(uint8_t majorVersion) = 0;

  virtual bool isSynchronized() = 0;
};

}
