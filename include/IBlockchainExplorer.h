// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <vector>
#include <array>

#include "BlockchainExplorerData.h"

namespace CryptoNote {

class IBlockchainObserver {
public:
  virtual ~IBlockchainObserver() {}

  virtual void blockchainUpdated(const std::vector<BlockDetails>& newBlocks, const std::vector<BlockDetails>& orphanedBlocks) {}
  virtual void poolUpdated(const std::vector<TransactionDetails>& newTransactions, const std::vector<std::pair<Crypto::Hash, TransactionRemoveReason>>& removedTransactions) {}

  virtual void blockchainSynchronized(const BlockDetails& topBlock) {}
};

class IBlockchainExplorer {
public:
  virtual ~IBlockchainExplorer() {};

  virtual bool addObserver(IBlockchainObserver* observer) = 0;
  virtual bool removeObserver(IBlockchainObserver* observer) = 0;

  virtual void init() = 0;
  virtual void shutdown() = 0;

  virtual bool getBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks) = 0;
  virtual bool getBlocks(const std::vector<Crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks) = 0;
  virtual bool getBlocks(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<BlockDetails>& blocks, uint32_t& blocksNumberWithinTimestamps) = 0;

  virtual bool getBlockchainTop(BlockDetails& topBlock) = 0;

  virtual bool getTransactions(const std::vector<Crypto::Hash>& transactionHashes, std::vector<TransactionDetails>& transactions) = 0;
  virtual bool getTransactionsByPaymentId(const Crypto::Hash& paymentId, std::vector<TransactionDetails>& transactions) = 0;
  virtual bool getPoolTransactions(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<TransactionDetails>& transactions, uint64_t& transactionsNumberWithinTimestamps) = 0;
  virtual bool getPoolState(const std::vector<Crypto::Hash>& knownPoolTransactionHashes, Crypto::Hash knownBlockchainTop, bool& isBlockchainActual, std::vector<TransactionDetails>& newTransactions, std::vector<Crypto::Hash>& removedTransactions) = 0;

  virtual uint64_t getRewardBlocksWindow() = 0;
  virtual uint64_t getFullRewardMaxBlockSize(uint8_t majorVersion) = 0;

  virtual bool isSynchronized() = 0;
};

}
