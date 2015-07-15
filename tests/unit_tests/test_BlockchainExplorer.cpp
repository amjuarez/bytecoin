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

#include "gtest/gtest.h"

#include <system_error>

#include <boost/range/combine.hpp>

#include "EventWaiter.h"
#include "ICoreStub.h"
#include "ICryptonoteProtocolQueryStub.h"
#include "INodeStubs.h"
#include "cryptonote_core/TransactionApi.h"
#include "TestBlockchainGenerator.h"

#include "Logging/FileLogger.h"

#include "BlockchainExplorer/BlockchainExplorer.h"

namespace {
CryptoNote::Transaction createTx(CryptoNote::ITransactionReader& tx) {
  auto data = tx.getTransactionData();

  CryptoNote::blobdata txblob(data.data(), data.data() + data.size());
  CryptoNote::Transaction outTx;
  CryptoNote::parse_and_validate_tx_from_blob(txblob, outTx);

  return outTx;
}
}

struct CallbackStatus {
  CallbackStatus() {}

  bool wait() { return waiter.wait_for(std::chrono::milliseconds(3000)); }
  bool ok() { return waiter.wait_for(std::chrono::milliseconds(3000)) && !static_cast<bool>(code); }
  void setStatus(const std::error_code& ec) { code = ec; waiter.notify(); }
  std::error_code getStatus() const { return code; }

  std::error_code code;
  EventWaiter waiter;
};

class dummyObserver : public CryptoNote::IBlockchainObserver {
public:
  virtual ~dummyObserver() {}
};

class smartObserver : public CryptoNote::IBlockchainObserver {
public:
  virtual ~smartObserver() {}

  virtual void blockchainUpdated(const std::vector<CryptoNote::BlockDetails>& newBlocks, const std::vector<CryptoNote::BlockDetails>& orphanedBlocks) override {
    blockchainUpdatedCallback(newBlocks, orphanedBlocks);
  }

  virtual void poolUpdated(const std::vector<CryptoNote::TransactionDetails>& newTransactions, const std::vector<std::pair<std::array<uint8_t, 32>, CryptoNote::TransactionRemoveReason>>& removedTransactions) override {
    poolUpdatedCallback(newTransactions, removedTransactions);
  }

  virtual void blockchainSynchronized(const CryptoNote::BlockDetails& topBlock) override {
    blockchainSynchronizedCallback(topBlock);
  }

  void setCallback(const std::function<void(const std::vector<CryptoNote::BlockDetails>& newBlocks, const std::vector<CryptoNote::BlockDetails>& orphanedBlocks)>& cb) {
    blockchainUpdatedCallback = cb;
  }

  void setCallback(const std::function<void(const std::vector<CryptoNote::TransactionDetails>& newTransactions, const std::vector<std::pair<std::array<uint8_t, 32>, CryptoNote::TransactionRemoveReason>>& removedTransactions)>& cb) {
    poolUpdatedCallback = cb;
  }

  void setCallback(std::function<void(const CryptoNote::BlockDetails& topBlock)>& cb) {
    blockchainSynchronizedCallback = cb;
  }

private:
  std::function<void(const std::vector<CryptoNote::BlockDetails>& newBlocks, const std::vector<CryptoNote::BlockDetails>& orphanedBlocks)> blockchainUpdatedCallback;
  std::function<void(const std::vector<CryptoNote::TransactionDetails>& newTransactions, const std::vector<std::pair<std::array<uint8_t, 32>, CryptoNote::TransactionRemoveReason>>& removedTransactions)> poolUpdatedCallback;
  std::function<void(const CryptoNote::BlockDetails& topBlock)> blockchainSynchronizedCallback;
};

class BlockchainExplorer : public ::testing::Test {
public:
  BlockchainExplorer() :
    currency(CryptoNote::CurrencyBuilder(logger).currency()),
    generator(currency),
    nodeStub(generator),
    blockchainExplorer(nodeStub, logger) {}
  void SetUp();
  void TearDown();

protected:
  CryptoNote::Currency currency;
  TestBlockchainGenerator generator;
  INodeTrivialRefreshStub nodeStub;
  Logging::FileLogger logger;
  dummyObserver observer;
  CryptoNote::BlockchainExplorer blockchainExplorer;
};

void BlockchainExplorer::SetUp() {
  logger.init("/dev/null");
  ASSERT_NO_THROW(blockchainExplorer.init());
}

void BlockchainExplorer::TearDown() {
  ASSERT_NO_THROW(blockchainExplorer.shutdown());
}

TEST_F(BlockchainExplorer, initOk) {
  CryptoNote::BlockchainExplorer newExplorer(nodeStub, logger);

  ASSERT_NO_THROW(newExplorer.init());
}

TEST_F(BlockchainExplorer, shutdownOk) {
  CryptoNote::BlockchainExplorer newExplorer(nodeStub, logger);
  ASSERT_NO_THROW(newExplorer.init());
  ASSERT_NO_THROW(newExplorer.shutdown());
}

TEST_F(BlockchainExplorer, doubleInit) {
  CryptoNote::BlockchainExplorer newExplorer(nodeStub, logger);
  ASSERT_NO_THROW(newExplorer.init());
  ASSERT_ANY_THROW(newExplorer.init());
}

TEST_F(BlockchainExplorer, shutdownNotInited) {
  CryptoNote::BlockchainExplorer newExplorer(nodeStub, logger);
  ASSERT_ANY_THROW(newExplorer.shutdown());
}

TEST_F(BlockchainExplorer, addObserver) {
  ASSERT_TRUE(blockchainExplorer.addObserver(&observer));
}

TEST_F(BlockchainExplorer, addObserverNotInited) {
  CryptoNote::BlockchainExplorer newExplorer(nodeStub, logger);
  ASSERT_ANY_THROW(newExplorer.addObserver(&observer));
}

TEST_F(BlockchainExplorer, removeObserver) {
  ASSERT_TRUE(blockchainExplorer.addObserver(&observer));
  ASSERT_TRUE(blockchainExplorer.removeObserver(&observer));
}

TEST_F(BlockchainExplorer, removeObserverNotInited) {
  CryptoNote::BlockchainExplorer newExplorer(nodeStub, logger);
  ASSERT_ANY_THROW(newExplorer.addObserver(&observer));
  ASSERT_ANY_THROW(newExplorer.removeObserver(&observer));
}

TEST_F(BlockchainExplorer, removeObserverNotAdded) {
  ASSERT_FALSE(blockchainExplorer.removeObserver(&observer));
}

TEST_F(BlockchainExplorer, getBlocksByHeightGenesis) {
  std::vector<uint64_t> blockHeights;
  blockHeights.push_back(0);
  std::vector<std::vector<CryptoNote::BlockDetails>> blocks;

  ASSERT_GE(generator.getBlockchain().size(), 1);

  ASSERT_TRUE(blockchainExplorer.getBlocks(blockHeights, blocks));
  ASSERT_EQ(blocks.size(), 1);
  EXPECT_EQ(blockHeights.size(), blocks.size());
  ASSERT_EQ(blocks.front().size(), 1);
  EXPECT_EQ(blocks.front().front().height, 0);
  EXPECT_FALSE(blocks.front().front().isOrphaned);
}

TEST_F(BlockchainExplorer, getBlocksByHeightMany) {
  const size_t NUMBER_OF_BLOCKS = 10;
  std::vector<uint64_t> blockHeights;
  for (size_t i = 0; i < NUMBER_OF_BLOCKS; ++i) {
    blockHeights.push_back(i);
  }
  std::vector<std::vector<CryptoNote::BlockDetails>> blocks;

  generator.generateEmptyBlocks(NUMBER_OF_BLOCKS);
  ASSERT_GE(generator.getBlockchain().size(), NUMBER_OF_BLOCKS);

  ASSERT_TRUE(blockchainExplorer.getBlocks(blockHeights, blocks));
  EXPECT_EQ(blocks.size(), NUMBER_OF_BLOCKS);
  ASSERT_EQ(blockHeights.size(), blocks.size());

  auto range = boost::combine(blockHeights, blocks);
  for (const boost::tuple<size_t, std::vector<CryptoNote::BlockDetails>>& sameHeight : range) {
    EXPECT_EQ(sameHeight.get<1>().size(), 1);
    for (const CryptoNote::BlockDetails& block : sameHeight.get<1>()) {
      EXPECT_EQ(block.height, sameHeight.get<0>());
      EXPECT_FALSE(block.isOrphaned);
    }
  }
}

TEST_F(BlockchainExplorer, getBlocksByHeightFail) {
  const size_t NUMBER_OF_BLOCKS = 10;
  std::vector<uint64_t> blockHeights;
  for (size_t i = 0; i < NUMBER_OF_BLOCKS; ++i) {
    blockHeights.push_back(i);
  }
  std::vector<std::vector<CryptoNote::BlockDetails>> blocks;

  EXPECT_LT(generator.getBlockchain().size(), NUMBER_OF_BLOCKS);

  ASSERT_ANY_THROW(blockchainExplorer.getBlocks(blockHeights, blocks));
}

TEST_F(BlockchainExplorer, getBlocksByHeightNotInited) {
  CryptoNote::BlockchainExplorer newExplorer(nodeStub, logger);
  std::vector<uint64_t> blockHeights;
  blockHeights.push_back(0);
  std::vector<std::vector<CryptoNote::BlockDetails>> blocks;
  ASSERT_ANY_THROW(newExplorer.getBlocks(blockHeights, blocks));
}

TEST_F(BlockchainExplorer, getBlocksByHashGenesis) {
  std::vector<std::array<uint8_t, 32>> blockHashes;
  ASSERT_GE(generator.getBlockchain().size(), 1);

  crypto::hash genesisHash = CryptoNote::get_block_hash(generator.getBlockchain().front());
  blockHashes.push_back(reinterpret_cast<const std::array<uint8_t, 32>&>(genesisHash));
  std::vector<CryptoNote::BlockDetails> blocks;

  ASSERT_TRUE(blockchainExplorer.getBlocks(blockHashes, blocks));
  ASSERT_EQ(blocks.size(), 1);
  EXPECT_EQ(blockHashes.size(), blocks.size());

  std::array<uint8_t, 32> expectedHash = reinterpret_cast<const std::array<uint8_t, 32>&>(genesisHash);
  EXPECT_EQ(blocks.front().hash, expectedHash);
  EXPECT_EQ(blocks.front().hash, blockHashes.front());
  EXPECT_FALSE(blocks.front().isOrphaned);
}

TEST_F(BlockchainExplorer, getBlocksByHashMany) {
  const size_t NUMBER_OF_BLOCKS = 10;
  std::vector<std::array<uint8_t, 32>> blockHashes;

  generator.generateEmptyBlocks(NUMBER_OF_BLOCKS);
  ASSERT_GE(generator.getBlockchain().size(), NUMBER_OF_BLOCKS);

  for (const auto& block : generator.getBlockchain()) {
    if (blockHashes.size() == NUMBER_OF_BLOCKS) {
      break;
    }
    crypto::hash hash = CryptoNote::get_block_hash(block);
    blockHashes.push_back(reinterpret_cast<const std::array<uint8_t, 32>&>(hash));
  }

  std::vector<CryptoNote::BlockDetails> blocks;

  ASSERT_TRUE(blockchainExplorer.getBlocks(blockHashes, blocks));
  EXPECT_EQ(blocks.size(), NUMBER_OF_BLOCKS);
  ASSERT_EQ(blockHashes.size(), blocks.size());

  auto range = boost::combine(blockHashes, blocks);
  for (const boost::tuple<std::array<uint8_t, 32>, CryptoNote::BlockDetails>& hashWithBlock : range) {
    EXPECT_EQ(hashWithBlock.get<0>(), hashWithBlock.get<1>().hash);
    EXPECT_FALSE(hashWithBlock.get<1>().isOrphaned);
  }
}

TEST_F(BlockchainExplorer, getBlocksByHashFail) {
  const size_t NUMBER_OF_BLOCKS = 10;
  std::vector<std::array<uint8_t, 32>> blockHashes;

  for (size_t i = 0; i < NUMBER_OF_BLOCKS; ++i) {
    blockHashes.push_back(boost::value_initialized<std::array<uint8_t, 32>>());
  }

  std::vector<CryptoNote::BlockDetails> blocks;

  EXPECT_LT(generator.getBlockchain().size(), NUMBER_OF_BLOCKS);
  ASSERT_ANY_THROW(blockchainExplorer.getBlocks(blockHashes, blocks));

}

TEST_F(BlockchainExplorer, getBlocksByHashNotInited) {
  CryptoNote::BlockchainExplorer newExplorer(nodeStub, logger);
  std::vector<std::array<uint8_t, 32>> blockHashes;
  crypto::hash genesisHash = CryptoNote::get_block_hash(generator.getBlockchain().front());
  blockHashes.push_back(reinterpret_cast<const std::array<uint8_t, 32>&>(genesisHash));
  std::vector<CryptoNote::BlockDetails> blocks;
  ASSERT_ANY_THROW(newExplorer.getBlocks(blockHashes, blocks));
}

TEST_F(BlockchainExplorer, getBlockchainTop) {
  CryptoNote::BlockDetails topBlock;

  ASSERT_GE(generator.getBlockchain().size(), 1);

  ASSERT_TRUE(blockchainExplorer.getBlockchainTop(topBlock));
  EXPECT_EQ(topBlock.height, generator.getBlockchain().size() - 1);
  EXPECT_FALSE(topBlock.isOrphaned);
}

TEST_F(BlockchainExplorer, getBlockchainTopNotInited) {
  CryptoNote::BlockchainExplorer newExplorer(nodeStub, logger);
  CryptoNote::BlockDetails topBlock;

  ASSERT_GE(generator.getBlockchain().size(), 1);

  ASSERT_ANY_THROW(newExplorer.getBlockchainTop(topBlock));
}

TEST_F(BlockchainExplorer, getTransactionFromBlockchain) {
  auto txptr = CryptoNote::createTransaction();
  auto tx = ::createTx(*txptr.get());
  generator.addTxToBlockchain(tx);

  ASSERT_GE(generator.getBlockchain().size(), 1);

  std::vector<std::array<uint8_t, 32>> transactionHashes;
  crypto::hash hash = CryptoNote::get_transaction_hash(tx);
  transactionHashes.push_back(reinterpret_cast<const std::array<uint8_t, 32>&>(hash));

  std::vector<CryptoNote::TransactionDetails> transactions;

  ASSERT_TRUE(blockchainExplorer.getTransactions(transactionHashes, transactions));
  ASSERT_EQ(transactions.size(), 1);
  EXPECT_EQ(transactions.size(), transactionHashes.size());

  EXPECT_EQ(transactions.front().hash, transactionHashes.front());
  EXPECT_TRUE(transactions.front().inBlockchain);
}

TEST_F(BlockchainExplorer, getTransactionFromPool) {
  auto txptr = CryptoNote::createTransaction();
  auto tx = ::createTx(*txptr.get());
  generator.putTxToPool(tx);

  ASSERT_GE(generator.getBlockchain().size(), 1);

  std::vector<std::array<uint8_t, 32>> transactionHashes;
  crypto::hash hash = CryptoNote::get_transaction_hash(tx);
  transactionHashes.push_back(reinterpret_cast<const std::array<uint8_t, 32>&>(hash));

  std::vector<CryptoNote::TransactionDetails> transactions;

  ASSERT_TRUE(blockchainExplorer.getTransactions(transactionHashes, transactions));
  ASSERT_EQ(transactions.size(), 1);
  EXPECT_EQ(transactions.size(), transactionHashes.size());

  EXPECT_EQ(transactions.front().hash, transactionHashes.front());
  EXPECT_FALSE(transactions.front().inBlockchain);
}

TEST_F(BlockchainExplorer, getTransactionsMany) {
  size_t POOL_TX_NUMBER = 10;
  size_t BLOCKCHAIN_TX_NUMBER = 10;
  std::vector<std::array<uint8_t, 32>> poolTxs;
  std::vector<std::array<uint8_t, 32>> blockchainTxs;

  for (size_t i = 0; i < POOL_TX_NUMBER; ++i) {
    auto txptr = CryptoNote::createTransaction();
    auto tx = ::createTx(*txptr.get());
    crypto::hash hash = CryptoNote::get_transaction_hash(tx);
    poolTxs.push_back(reinterpret_cast<const std::array<uint8_t, 32>&>(hash));
    generator.putTxToPool(tx);
  }

  for (size_t i = 0; i < BLOCKCHAIN_TX_NUMBER; ++i) {
    auto txptr = CryptoNote::createTransaction();
    auto tx = ::createTx(*txptr.get());
    crypto::hash hash = CryptoNote::get_transaction_hash(tx);
    blockchainTxs.push_back(reinterpret_cast<const std::array<uint8_t, 32>&>(hash));
    generator.addTxToBlockchain(tx);
  }

  ASSERT_GE(generator.getBlockchain().size(), 1);

  std::vector<std::array<uint8_t, 32>> transactionHashes;
  std::copy(poolTxs.begin(), poolTxs.end(), std::back_inserter(transactionHashes));
  std::copy(blockchainTxs.begin(), blockchainTxs.end(), std::back_inserter(transactionHashes));

  std::vector<CryptoNote::TransactionDetails> transactions;

  ASSERT_TRUE(blockchainExplorer.getTransactions(transactionHashes, transactions));
  ASSERT_EQ(transactions.size(), POOL_TX_NUMBER + BLOCKCHAIN_TX_NUMBER);
  EXPECT_EQ(transactions.size(), transactionHashes.size());

  for (const std::array<uint8_t, 32>& poolTxHash : poolTxs) {
    auto iter = std::find_if(
        transactions.begin(), 
        transactions.end(), 
        [&poolTxHash](const CryptoNote::TransactionDetails& txDetails) -> bool {
          return poolTxHash == txDetails.hash;
        }
    );
    EXPECT_NE(iter, transactions.end());
    EXPECT_EQ(iter->hash, poolTxHash);
    EXPECT_FALSE(iter->inBlockchain);
  }

  for (const std::array<uint8_t, 32>& blockchainTxHash : blockchainTxs) {
    auto iter = std::find_if(
        transactions.begin(), 
        transactions.end(), 
        [&blockchainTxHash](const CryptoNote::TransactionDetails& txDetails) -> bool {
          return blockchainTxHash == txDetails.hash;
        }
    );
    EXPECT_NE(iter, transactions.end());
    EXPECT_EQ(iter->hash, blockchainTxHash);
    EXPECT_TRUE(iter->inBlockchain);
  }
}

TEST_F(BlockchainExplorer, getTransactionsFail) {
  size_t POOL_TX_NUMBER = 10;
  size_t BLOCKCHAIN_TX_NUMBER = 10;
  std::vector<std::array<uint8_t, 32>> poolTxs;
  std::vector<std::array<uint8_t, 32>> blockchainTxs;

  for (size_t i = 0; i < POOL_TX_NUMBER; ++i) {
    auto txptr = CryptoNote::createTransaction();
    auto tx = ::createTx(*txptr.get());
    crypto::hash hash = CryptoNote::get_transaction_hash(tx);
    poolTxs.push_back(reinterpret_cast<const std::array<uint8_t, 32>&>(hash));
    generator.putTxToPool(tx);
  }

  for (size_t i = 0; i < BLOCKCHAIN_TX_NUMBER; ++i) {
    auto txptr = CryptoNote::createTransaction();
    auto tx = ::createTx(*txptr.get());
    crypto::hash hash = CryptoNote::get_transaction_hash(tx);
    blockchainTxs.push_back(reinterpret_cast<const std::array<uint8_t, 32>&>(hash));
    generator.addTxToBlockchain(tx);
  }

  ASSERT_GE(generator.getBlockchain().size(), 1);

  std::vector<std::array<uint8_t, 32>> transactionHashes;
  transactionHashes.push_back(boost::value_initialized<std::array<uint8_t, 32>>());

  std::vector<CryptoNote::TransactionDetails> transactions;

  ASSERT_ANY_THROW(blockchainExplorer.getTransactions(transactionHashes, transactions));
}

TEST_F(BlockchainExplorer, getTransactionsNotInited) {
  auto txptr = CryptoNote::createTransaction();
  auto tx = ::createTx(*txptr.get());
  generator.addTxToBlockchain(tx);

  ASSERT_GE(generator.getBlockchain().size(), 1);

  std::vector<std::array<uint8_t, 32>> transactionHashes;
  crypto::hash hash = CryptoNote::get_transaction_hash(tx);
  transactionHashes.push_back(reinterpret_cast<const std::array<uint8_t, 32>&>(hash));

  std::vector<CryptoNote::TransactionDetails> transactions;

  CryptoNote::BlockchainExplorer newExplorer(nodeStub, logger);

  ASSERT_ANY_THROW(newExplorer.getTransactions(transactionHashes, transactions));
}

TEST_F(BlockchainExplorer, getPoolStateEmpty) {
  CryptoNote::BlockDetails topBlock;

  ASSERT_GE(generator.getBlockchain().size(), 1);

  ASSERT_TRUE(blockchainExplorer.getBlockchainTop(topBlock));
  EXPECT_EQ(topBlock.height, generator.getBlockchain().size() - 1);
  EXPECT_FALSE(topBlock.isOrphaned);

  std::vector<std::array<uint8_t, 32>> knownPoolTransactionHashes;
  std::array<uint8_t, 32> knownBlockchainTop = topBlock.hash;
  bool isBlockchainActual;

  std::vector<CryptoNote::TransactionDetails> newTransactions;
  std::vector<std::array<uint8_t, 32>> removedTransactions;

  ASSERT_TRUE(blockchainExplorer.getPoolState(knownPoolTransactionHashes, knownBlockchainTop, isBlockchainActual, newTransactions, removedTransactions));
  EXPECT_TRUE(isBlockchainActual);

  EXPECT_EQ(newTransactions.size(), 0);
  EXPECT_EQ(removedTransactions.size(), 0);
}

TEST_F(BlockchainExplorer, getPoolStateMany) {
  size_t POOL_TX_NUMBER = 10;
  std::vector<std::array<uint8_t, 32>> poolTxs;

  for (size_t i = 0; i < POOL_TX_NUMBER; ++i) {
    auto txptr = CryptoNote::createTransaction();
    auto tx = ::createTx(*txptr.get());
    crypto::hash hash = CryptoNote::get_transaction_hash(tx);
    poolTxs.push_back(reinterpret_cast<const std::array<uint8_t, 32>&>(hash));
    generator.putTxToPool(tx);
  }

  {
    CryptoNote::BlockDetails topBlock;

    ASSERT_GE(generator.getBlockchain().size(), 1);

    ASSERT_TRUE(blockchainExplorer.getBlockchainTop(topBlock));
    EXPECT_EQ(topBlock.height, generator.getBlockchain().size() - 1);
    EXPECT_FALSE(topBlock.isOrphaned);

    std::vector<std::array<uint8_t, 32>> knownPoolTransactionHashes;
    std::array<uint8_t, 32> knownBlockchainTop = topBlock.hash;
    bool isBlockchainActual;

    std::vector<CryptoNote::TransactionDetails> newTransactions;
    std::vector<std::array<uint8_t, 32>> removedTransactions;

    ASSERT_TRUE(blockchainExplorer.getPoolState(knownPoolTransactionHashes, knownBlockchainTop, isBlockchainActual, newTransactions, removedTransactions));
    EXPECT_TRUE(isBlockchainActual);

    EXPECT_EQ(newTransactions.size(), POOL_TX_NUMBER);
    EXPECT_EQ(removedTransactions.size(), 0);

    for (const std::array<uint8_t, 32>& poolTxHash : poolTxs) {
      auto iter = std::find_if(
          newTransactions.begin(), 
          newTransactions.end(), 
          [&poolTxHash](const CryptoNote::TransactionDetails& txDetails) -> bool {
            return poolTxHash == txDetails.hash;
          }
      );
      EXPECT_NE(iter, newTransactions.end());
      EXPECT_EQ(iter->hash, poolTxHash);
      EXPECT_FALSE(iter->inBlockchain);
    }
  }

  generator.putTxPoolToBlockchain();

  {
    CryptoNote::BlockDetails topBlock;

    ASSERT_GE(generator.getBlockchain().size(), 1);

    ASSERT_TRUE(blockchainExplorer.getBlockchainTop(topBlock));
    EXPECT_EQ(topBlock.height, generator.getBlockchain().size() - 1);
    EXPECT_FALSE(topBlock.isOrphaned);

    std::vector<std::array<uint8_t, 32>> knownPoolTransactionHashes;
    std::array<uint8_t, 32> knownBlockchainTop = topBlock.hash;
    bool isBlockchainActual;

    std::vector<CryptoNote::TransactionDetails> newTransactions;
    std::vector<std::array<uint8_t, 32>> removedTransactions;

    ASSERT_TRUE(blockchainExplorer.getPoolState(knownPoolTransactionHashes, knownBlockchainTop, isBlockchainActual, newTransactions, removedTransactions));
    EXPECT_TRUE(isBlockchainActual);

    EXPECT_EQ(newTransactions.size(), 0);
    EXPECT_EQ(removedTransactions.size(), 0);
  }

  {
    CryptoNote::BlockDetails topBlock;

    ASSERT_GE(generator.getBlockchain().size(), 1);

    ASSERT_TRUE(blockchainExplorer.getBlockchainTop(topBlock));
    EXPECT_EQ(topBlock.height, generator.getBlockchain().size() - 1);
    EXPECT_FALSE(topBlock.isOrphaned);

    std::vector<std::array<uint8_t, 32>> knownPoolTransactionHashes = poolTxs;
    std::array<uint8_t, 32> knownBlockchainTop = topBlock.hash;
    bool isBlockchainActual;

    std::vector<CryptoNote::TransactionDetails> newTransactions;
    std::vector<std::array<uint8_t, 32>> removedTransactions;

    ASSERT_TRUE(blockchainExplorer.getPoolState(knownPoolTransactionHashes, knownBlockchainTop, isBlockchainActual, newTransactions, removedTransactions));
    EXPECT_TRUE(isBlockchainActual);

    EXPECT_EQ(newTransactions.size(), 0);
    EXPECT_EQ(removedTransactions.size(), POOL_TX_NUMBER);

    for (const std::array<uint8_t, 32>& poolTxHash : knownPoolTransactionHashes) {
      auto iter = std::find(
          removedTransactions.begin(), 
          removedTransactions.end(), 
          poolTxHash
      );
      EXPECT_NE(iter, removedTransactions.end());
      EXPECT_EQ(*iter, poolTxHash);
    }
  }

  auto txptr = CryptoNote::createTransaction();
  auto tx = ::createTx(*txptr.get());
  crypto::hash hash = CryptoNote::get_transaction_hash(tx);
  std::array<uint8_t, 32> newTxHash = reinterpret_cast<const std::array<uint8_t, 32>&>(hash);
  generator.putTxToPool(tx);

  {
    CryptoNote::BlockDetails topBlock;

    ASSERT_GE(generator.getBlockchain().size(), 1);

    ASSERT_TRUE(blockchainExplorer.getBlockchainTop(topBlock));
    EXPECT_EQ(topBlock.height, generator.getBlockchain().size() - 1);
    EXPECT_FALSE(topBlock.isOrphaned);

    std::vector<std::array<uint8_t, 32>> knownPoolTransactionHashes = poolTxs;
    std::array<uint8_t, 32> knownBlockchainTop = topBlock.hash;
    bool isBlockchainActual;

    std::vector<CryptoNote::TransactionDetails> newTransactions;
    std::vector<std::array<uint8_t, 32>> removedTransactions;

    ASSERT_TRUE(blockchainExplorer.getPoolState(knownPoolTransactionHashes, knownBlockchainTop, isBlockchainActual, newTransactions, removedTransactions));
    EXPECT_TRUE(isBlockchainActual);

    ASSERT_EQ(newTransactions.size(), 1);
    EXPECT_EQ(newTransactions.front().hash, newTxHash);
    EXPECT_EQ(removedTransactions.size(), POOL_TX_NUMBER);

    for (const std::array<uint8_t, 32>& poolTxHash : knownPoolTransactionHashes) {
      auto iter = std::find(
          removedTransactions.begin(), 
          removedTransactions.end(), 
          poolTxHash
      );
      EXPECT_NE(iter, removedTransactions.end());
      EXPECT_EQ(*iter, poolTxHash);
    }
  }

  {
    CryptoNote::BlockDetails topBlock;

    ASSERT_GE(generator.getBlockchain().size(), 1);

    std::vector<std::array<uint8_t, 32>> knownPoolTransactionHashes;
    std::array<uint8_t, 32> knownBlockchainTop = boost::value_initialized<std::array<uint8_t, 32>>();
    bool isBlockchainActual;

    std::vector<CryptoNote::TransactionDetails> newTransactions;
    std::vector<std::array<uint8_t, 32>> removedTransactions;

    ASSERT_TRUE(blockchainExplorer.getPoolState(knownPoolTransactionHashes, knownBlockchainTop, isBlockchainActual, newTransactions, removedTransactions));
    EXPECT_FALSE(isBlockchainActual);
  }
} 

TEST_F(BlockchainExplorer, getPoolStateNotInited) {

  std::vector<std::array<uint8_t, 32>> knownPoolTransactionHashes;
  std::array<uint8_t, 32> knownBlockchainTop = boost::value_initialized<std::array<uint8_t, 32>>();
  bool isBlockchainActual;

  std::vector<CryptoNote::TransactionDetails> newTransactions;
  std::vector<std::array<uint8_t, 32>> removedTransactions;

  CryptoNote::BlockchainExplorer newExplorer(nodeStub, logger);

  ASSERT_ANY_THROW(newExplorer.getPoolState(knownPoolTransactionHashes, knownBlockchainTop, isBlockchainActual, newTransactions, removedTransactions));
}

TEST_F(BlockchainExplorer, getRewardBlocksWindow) {
  ASSERT_EQ(blockchainExplorer.getRewardBlocksWindow(), CryptoNote::parameters::CRYPTONOTE_REWARD_BLOCKS_WINDOW);
}

TEST_F(BlockchainExplorer, getRewardBlocksWindowNotInited) {
  CryptoNote::BlockchainExplorer newExplorer(nodeStub, logger);
  ASSERT_ANY_THROW(newExplorer.getRewardBlocksWindow());
}

TEST_F(BlockchainExplorer, getFullRewardMaxBlockSize) {
  ASSERT_EQ(blockchainExplorer.getFullRewardMaxBlockSize(1), CryptoNote::parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1);
  ASSERT_EQ(blockchainExplorer.getFullRewardMaxBlockSize(2), CryptoNote::parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE);
}

TEST_F(BlockchainExplorer, getFullRewardMaxBlockSizeNotInited) {
  CryptoNote::BlockchainExplorer newExplorer(nodeStub, logger);
  ASSERT_ANY_THROW(newExplorer.getFullRewardMaxBlockSize(1));
}

TEST_F(BlockchainExplorer, isSynchronizedFalse) {
  ASSERT_FALSE(blockchainExplorer.isSynchronized());
}

TEST_F(BlockchainExplorer, isSynchronizedNotInited) {
  CryptoNote::BlockchainExplorer newExplorer(nodeStub, logger);
  ASSERT_ANY_THROW(newExplorer.isSynchronized());
}

TEST_F(BlockchainExplorer, isSynchronizedNotification) {
  smartObserver observer;
  CallbackStatus status;

  std::function<void(const CryptoNote::BlockDetails& topBlock)> cb = [&status, this](const CryptoNote::BlockDetails& topBlock){
    EXPECT_EQ(topBlock.height, generator.getBlockchain().size() - 1);
    status.setStatus(std::error_code());
  };
  observer.setCallback(cb);
  blockchainExplorer.addObserver(&observer);

  nodeStub.setSynchronizedStatus(true);
  ASSERT_TRUE(blockchainExplorer.isSynchronized());
  ASSERT_TRUE(status.wait());
}

TEST_F(BlockchainExplorer, blockchainUpdatedEmpty) {
  smartObserver observer;
  CallbackStatus status;

  std::function<
    void(const std::vector<CryptoNote::BlockDetails>& newBlocks, 
      const std::vector<CryptoNote::BlockDetails>& orphanedBlocks)
    > cb = [&status, this](const std::vector<CryptoNote::BlockDetails>& newBlocks, 
      const std::vector<CryptoNote::BlockDetails>& orphanedBlocks) {
    EXPECT_EQ(newBlocks.size(), 0);
    EXPECT_EQ(orphanedBlocks.size(), 0);
    status.setStatus(std::error_code());
  };
  observer.setCallback(cb);
  blockchainExplorer.addObserver(&observer);

  nodeStub.sendLocalBlockchainUpdated();
  ASSERT_TRUE(status.wait());
}

TEST_F(BlockchainExplorer, blockchainUpdatedMany) {
  const size_t NUMBER_OF_BLOCKS = 10;
  std::vector<std::array<uint8_t, 32>> blockHashes;

  generator.generateEmptyBlocks(NUMBER_OF_BLOCKS);
  ASSERT_GE(generator.getBlockchain().size(), NUMBER_OF_BLOCKS);

  for (auto iter = generator.getBlockchain().begin() + 2; iter != generator.getBlockchain().end(); iter++) {
    if (blockHashes.size() == NUMBER_OF_BLOCKS) {
      break;
    }
    crypto::hash hash = CryptoNote::get_block_hash(*iter);
    blockHashes.push_back(reinterpret_cast<const std::array<uint8_t, 32>&>(hash));
  }

  smartObserver observer;
  CallbackStatus status;

  std::function<
    void(const std::vector<CryptoNote::BlockDetails>& newBlocks, 
      const std::vector<CryptoNote::BlockDetails>& orphanedBlocks)
    > cb = [&status, &blockHashes, this, NUMBER_OF_BLOCKS](const std::vector<CryptoNote::BlockDetails>& newBlocks, 
      const std::vector<CryptoNote::BlockDetails>& orphanedBlocks) {
    EXPECT_EQ(newBlocks.size(), NUMBER_OF_BLOCKS);
    EXPECT_EQ(orphanedBlocks.size(), 0);

    auto range = boost::combine(blockHashes, newBlocks);
    for (const boost::tuple<std::array<uint8_t, 32>, CryptoNote::BlockDetails>& hashWithBlock : range) {
      EXPECT_EQ(hashWithBlock.get<0>(), hashWithBlock.get<1>().hash);
      EXPECT_FALSE(hashWithBlock.get<1>().isOrphaned);
    }

    status.setStatus(std::error_code());
  };
  observer.setCallback(cb);
  blockchainExplorer.addObserver(&observer);

  nodeStub.sendLocalBlockchainUpdated();
  ASSERT_TRUE(status.wait());
}

TEST_F(BlockchainExplorer, poolUpdatedEmpty) {
  smartObserver observer;
  CallbackStatus status;

  std::function<
    void(const std::vector<CryptoNote::TransactionDetails>& newTransactions, 
      const std::vector<std::pair<std::array<uint8_t, 32>, CryptoNote::TransactionRemoveReason>>& removedTransactions)
    > cb = [&status, this](const std::vector<CryptoNote::TransactionDetails>& newTransactions, 
      const std::vector<std::pair<std::array<uint8_t, 32>, CryptoNote::TransactionRemoveReason>>& removedTransactions) {
    EXPECT_EQ(newTransactions.size(), 0);
    EXPECT_EQ(removedTransactions.size(), 0);
    status.setStatus(std::error_code());
  };
  observer.setCallback(cb);
  blockchainExplorer.addObserver(&observer);

  nodeStub.sendPoolChanged();
  ASSERT_FALSE(status.wait());
}

TEST_F(BlockchainExplorer, poolUpdatedMany) {
  size_t POOL_TX_NUMBER = 10;
  std::vector<std::array<uint8_t, 32>> poolTxs;

  for (size_t i = 0; i < POOL_TX_NUMBER; ++i) {
    auto txptr = CryptoNote::createTransaction();
    auto tx = ::createTx(*txptr.get());
    crypto::hash hash = CryptoNote::get_transaction_hash(tx);
    poolTxs.push_back(reinterpret_cast<const std::array<uint8_t, 32>&>(hash));
    generator.putTxToPool(tx);
  }
  {
    CryptoNote::BlockDetails topBlock;

    ASSERT_GE(generator.getBlockchain().size(), 1);

    ASSERT_TRUE(blockchainExplorer.getBlockchainTop(topBlock));
    EXPECT_EQ(topBlock.height, generator.getBlockchain().size() - 1);
    EXPECT_FALSE(topBlock.isOrphaned);

    smartObserver observer;
    CallbackStatus status;

    std::function<
      void(const std::vector<CryptoNote::TransactionDetails>& newTransactions, 
        const std::vector<std::pair<std::array<uint8_t, 32>, CryptoNote::TransactionRemoveReason>>& removedTransactions)
      > cb = [&status, &poolTxs, this, POOL_TX_NUMBER](const std::vector<CryptoNote::TransactionDetails>& newTransactions, 
        const std::vector<std::pair<std::array<uint8_t, 32>, CryptoNote::TransactionRemoveReason>>& removedTransactions) {
      EXPECT_EQ(newTransactions.size(), POOL_TX_NUMBER);
      EXPECT_EQ(removedTransactions.size(), 0);

      for (const std::array<uint8_t, 32>& poolTxHash : poolTxs) {
        auto iter = std::find_if(
            newTransactions.begin(), 
            newTransactions.end(), 
            [&poolTxHash](const CryptoNote::TransactionDetails& txDetails) -> bool {
              return poolTxHash == txDetails.hash;
            }
        );
        EXPECT_NE(iter, newTransactions.end());
        EXPECT_EQ(iter->hash, poolTxHash);
        EXPECT_FALSE(iter->inBlockchain);
      }
      status.setStatus(std::error_code());
    };
    observer.setCallback(cb);

    std::function<
      void(const std::vector<CryptoNote::BlockDetails>& newBlocks, 
        const std::vector<CryptoNote::BlockDetails>& orphanedBlocks)
      > cb1 = [&status, this](const std::vector<CryptoNote::BlockDetails>& newBlocks, 
        const std::vector<CryptoNote::BlockDetails>& orphanedBlocks) {};
    observer.setCallback(cb1);

    nodeStub.sendLocalBlockchainUpdated();

    blockchainExplorer.addObserver(&observer);

    nodeStub.sendPoolChanged();
    ASSERT_TRUE(status.wait());
    blockchainExplorer.removeObserver(&observer);
  }

  generator.putTxPoolToBlockchain();

  {
    CryptoNote::BlockDetails topBlock;

    ASSERT_GE(generator.getBlockchain().size(), 1);

    ASSERT_TRUE(blockchainExplorer.getBlockchainTop(topBlock));
    EXPECT_EQ(topBlock.height, generator.getBlockchain().size() - 1);
    EXPECT_FALSE(topBlock.isOrphaned);

    smartObserver observer;
    CallbackStatus status;
    CallbackStatus status1;

    std::function<
      void(const std::vector<CryptoNote::TransactionDetails>& newTransactions, 
        const std::vector<std::pair<std::array<uint8_t, 32>, CryptoNote::TransactionRemoveReason>>& removedTransactions)
      > cb = [&status, &poolTxs, this, POOL_TX_NUMBER](const std::vector<CryptoNote::TransactionDetails>& newTransactions, 
        const std::vector<std::pair<std::array<uint8_t, 32>, CryptoNote::TransactionRemoveReason>>& removedTransactions) {
      EXPECT_EQ(newTransactions.size(), 0);
      EXPECT_EQ(removedTransactions.size(), POOL_TX_NUMBER);

      for (const std::array<uint8_t, 32>& poolTxHash : poolTxs) {
        auto iter = std::find_if(
            removedTransactions.begin(), 
            removedTransactions.end(), 
            [&poolTxHash](const std::pair<std::array<uint8_t, 32>, CryptoNote::TransactionRemoveReason>& txDetails) -> bool {
              return poolTxHash == txDetails.first;
            }
        );
        EXPECT_NE(iter, removedTransactions.end());
        EXPECT_EQ(iter->first, poolTxHash);
        EXPECT_EQ(iter->second, CryptoNote::TransactionRemoveReason::INCLUDED_IN_BLOCK);
      }
      status.setStatus(std::error_code());
    };
    observer.setCallback(cb);

    std::function<
      void(const std::vector<CryptoNote::BlockDetails>& newBlocks, 
        const std::vector<CryptoNote::BlockDetails>& orphanedBlocks)
      > cb1 = [&status1, this](const std::vector<CryptoNote::BlockDetails>& newBlocks, 
        const std::vector<CryptoNote::BlockDetails>& orphanedBlocks) {
      status1.setStatus(std::error_code());
    };
    observer.setCallback(cb1);

    blockchainExplorer.addObserver(&observer);

    nodeStub.sendLocalBlockchainUpdated();
    ASSERT_TRUE(status1.wait());

    nodeStub.sendPoolChanged();
    ASSERT_TRUE(status.wait());
    blockchainExplorer.removeObserver(&observer);
  }

}
