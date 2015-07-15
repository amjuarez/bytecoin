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
#include "InProcessNode/InProcessNode.h"
#include "TestBlockchainGenerator.h"
#include "Logging/FileLogger.h"
#include "cryptonote_core/TransactionApi.h"

struct CallbackStatus {
  CallbackStatus() {}

  bool wait() { return waiter.wait_for(std::chrono::milliseconds(3000)); }
  bool ok() { return waiter.wait_for(std::chrono::milliseconds(3000)) && !static_cast<bool>(code); }
  void setStatus(const std::error_code& ec) { code = ec; waiter.notify(); }
  std::error_code getStatus() const { return code; }

  std::error_code code;
  EventWaiter waiter;
};

namespace {
CryptoNote::Transaction createTx(CryptoNote::ITransactionReader& tx) {
  auto data = tx.getTransactionData();

  CryptoNote::blobdata txblob(data.data(), data.data() + data.size());
  CryptoNote::Transaction outTx;
  CryptoNote::parse_and_validate_tx_from_blob(txblob, outTx);

  return outTx;
}
}

class InProcessNode : public ::testing::Test {
public:
  InProcessNode() : 
    node(coreStub, protocolQueryStub),
    currency(CryptoNote::CurrencyBuilder(logger).currency()),
    generator(currency) {}
  void SetUp();

protected:
  void initNode();

  ICoreStub coreStub;
  ICryptonoteProtocolQueryStub protocolQueryStub;
  CryptoNote::InProcessNode node;

  CryptoNote::Currency currency;
  TestBlockchainGenerator generator;
  Logging::FileLogger logger;
};

void InProcessNode::SetUp() {
  logger.init("/dev/null");
  initNode();
}

void InProcessNode::initNode() {
  CallbackStatus status;

  node.init([&status] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.ok());
}

TEST_F(InProcessNode, initOk) {
  CryptoNote::InProcessNode newNode(coreStub, protocolQueryStub);
  CallbackStatus status;

  newNode.init([&status] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.ok());
}

TEST_F(InProcessNode, doubleInit) {
  CallbackStatus status;
  node.init([&status] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.wait());

  std::error_code ec = status.getStatus();
  ASSERT_NE(ec, std::error_code());
}

TEST_F(InProcessNode, shutdownNotInited) {
  CryptoNote::InProcessNode newNode(coreStub, protocolQueryStub);
  ASSERT_FALSE(newNode.shutdown());
}

TEST_F(InProcessNode, shutdown) {
  ASSERT_TRUE(node.shutdown());
}

TEST_F(InProcessNode, getPeersCountSuccess) {
  protocolQueryStub.setPeerCount(1);
  ASSERT_EQ(1, node.getPeerCount());
}

TEST_F(InProcessNode, getLastLocalBlockHeightSuccess) {
  crypto::hash ignore;
  coreStub.set_blockchain_top(10, ignore, true);

  ASSERT_EQ(10, node.getLastLocalBlockHeight());
}

TEST_F(InProcessNode, getLastLocalBlockHeightFailure) {
  crypto::hash ignore;
  coreStub.set_blockchain_top(10, ignore, false);

  ASSERT_ANY_THROW(node.getLastLocalBlockHeight());
}

TEST_F(InProcessNode, getLastKnownBlockHeightSuccess) {
  protocolQueryStub.setObservedHeight(10);
  ASSERT_EQ(10, node.getLastKnownBlockHeight() + 1);
}

TEST_F(InProcessNode, getTransactionOutsGlobalIndicesSuccess) {
  crypto::hash ignore;
  std::vector<uint64_t> indices;
  std::vector<uint64_t> expectedIndices;

  uint64_t start = 10;
  std::generate_n(std::back_inserter(expectedIndices), 5, [&start] () { return start++; });
  coreStub.set_outputs_gindexs(expectedIndices, true);

  CallbackStatus status;
  node.getTransactionOutsGlobalIndices(ignore, indices, [&status] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.ok());

  ASSERT_EQ(expectedIndices.size(), indices.size());
  std::sort(indices.begin(), indices.end());
  ASSERT_TRUE(std::equal(indices.begin(), indices.end(), expectedIndices.begin()));
}

TEST_F(InProcessNode, getTransactionOutsGlobalIndicesFailure) {
  crypto::hash ignore;
  std::vector<uint64_t> indices;
  coreStub.set_outputs_gindexs(indices, false);

  CallbackStatus status;
  node.getTransactionOutsGlobalIndices(ignore, indices, [&status] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.wait());
  ASSERT_NE(std::error_code(), status.getStatus());
}

TEST_F(InProcessNode, getRandomOutsByAmountsSuccess) {
  crypto::public_key ignoredPublicKey;
  crypto::secret_key ignoredSectetKey;
  crypto::generate_keys(ignoredPublicKey, ignoredSectetKey);

  CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response expectedResp;
  CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_outs_for_amount out;
  out.amount = 10;
  out.outs.push_back({ 11, ignoredPublicKey });
  expectedResp.outs.push_back(out);
  coreStub.set_random_outs(expectedResp, true);

  std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_outs_for_amount> outs;

  CallbackStatus status;
  node.getRandomOutsByAmounts({1,2,3}, 1, outs, [&status] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.ok());
  ASSERT_EQ(1, outs.size());

  ASSERT_EQ(10, outs[0].amount);
  ASSERT_EQ(1, outs[0].outs.size());
  ASSERT_EQ(11, outs[0].outs.front().global_amount_index);
}

TEST_F(InProcessNode, getRandomOutsByAmountsFailure) {
  CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response expectedResp;
  coreStub.set_random_outs(expectedResp, false);

  std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_outs_for_amount> outs;

  CallbackStatus status;
  node.getRandomOutsByAmounts({1,2,3}, 1, outs, [&status] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.wait());
  ASSERT_NE(std::error_code(), status.getStatus());
}

TEST_F(InProcessNode, getPeerCountUninitialized) {
  CryptoNote::InProcessNode newNode(coreStub, protocolQueryStub);
  ASSERT_ANY_THROW(newNode.getPeerCount());
}

TEST_F(InProcessNode, getLastLocalBlockHeightUninitialized) {
  CryptoNote::InProcessNode newNode(coreStub, protocolQueryStub);
  ASSERT_ANY_THROW(newNode.getLastLocalBlockHeight());
}

TEST_F(InProcessNode, getLastKnownBlockHeightUninitialized) {
  CryptoNote::InProcessNode newNode(coreStub, protocolQueryStub);
  ASSERT_ANY_THROW(newNode.getLastKnownBlockHeight());
}

TEST_F(InProcessNode, getNewBlocksUninitialized) {
  CryptoNote::InProcessNode newNode(coreStub, protocolQueryStub);
  std::list<crypto::hash> knownBlockIds;
  std::list<CryptoNote::block_complete_entry> newBlocks;
  uint64_t startHeight;

  CallbackStatus status;
  newNode.getNewBlocks(std::move(knownBlockIds), newBlocks, startHeight, [&] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.wait());
  ASSERT_NE(std::error_code(), status.getStatus());
}

TEST_F(InProcessNode, getTransactionOutsGlobalIndicesUninitialized) {
  CryptoNote::InProcessNode newNode(coreStub, protocolQueryStub);
  std::vector<uint64_t> outsGlobalIndices;

  CallbackStatus status;
  newNode.getTransactionOutsGlobalIndices(crypto::hash(), outsGlobalIndices, [&] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.wait());
  ASSERT_NE(std::error_code(), status.getStatus());
}

TEST_F(InProcessNode, getRandomOutsByAmountsUninitialized) {
  CryptoNote::InProcessNode newNode(coreStub, protocolQueryStub);
  std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_outs_for_amount> outs;

  CallbackStatus status;
  newNode.getRandomOutsByAmounts({1,2,3}, 1, outs, [&] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.wait());
  ASSERT_NE(std::error_code(), status.getStatus());
}

TEST_F(InProcessNode, relayTransactionUninitialized) {
  CryptoNote::InProcessNode newNode(coreStub, protocolQueryStub);

  CallbackStatus status;
  newNode.relayTransaction(CryptoNote::Transaction(), [&] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.wait());
  ASSERT_NE(std::error_code(), status.getStatus());
}

TEST_F(InProcessNode, getLastLocalBlockTimestamp) {
  class GetBlockTimestampCore : public ICoreStub {
  public:
    GetBlockTimestampCore(uint64_t timestamp) : timestamp(timestamp) {}
    virtual bool get_blockchain_top(uint64_t& height, crypto::hash& top_id) override {
      return true;
    }

    virtual bool getBlockByHash(const crypto::hash &h, CryptoNote::Block &blk) override {
      blk.timestamp = timestamp;
      return true;
    }

    uint64_t timestamp;
  };

  uint64_t expectedTimestamp = 1234567890;
  GetBlockTimestampCore core(expectedTimestamp);
  CryptoNote::InProcessNode newNode(core, protocolQueryStub);

  CallbackStatus initStatus;
  newNode.init([&initStatus] (std::error_code ec) { initStatus.setStatus(ec); });
  ASSERT_TRUE(initStatus.wait());

  uint64_t timestamp = newNode.getLastLocalBlockTimestamp();

  ASSERT_EQ(expectedTimestamp, timestamp);
}

TEST_F(InProcessNode, getLastLocalBlockTimestampError) {
  class GetBlockTimestampErrorCore : public ICoreStub {
  public:
    virtual bool get_blockchain_top(uint64_t& height, crypto::hash& top_id) override {
      return true;
    }

    virtual bool getBlockByHash(const crypto::hash &h, CryptoNote::Block &blk) override {
      return false;
    }
  };

  GetBlockTimestampErrorCore core;
  CryptoNote::InProcessNode newNode(core, protocolQueryStub);

  CallbackStatus initStatus;
  newNode.init([&initStatus] (std::error_code ec) { initStatus.setStatus(ec); });
  ASSERT_TRUE(initStatus.wait());

  ASSERT_THROW(newNode.getLastLocalBlockTimestamp(), std::exception);
}

TEST_F(InProcessNode, getBlocksByHeightEmpty) {
  std::vector<uint64_t> blockHeights;
  std::vector<std::vector<CryptoNote::BlockDetails>> blocks;
  ASSERT_EQ(blockHeights.size(), 0);
  ASSERT_EQ(blocks.size(), 0);

  coreStub.set_blockchain_top(0, boost::value_initialized<crypto::hash>(), true);

  CallbackStatus status;
  node.getBlocks(blockHeights, blocks, [&status] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.wait());
  ASSERT_EQ(std::error_code(), status.getStatus());
}

TEST_F(InProcessNode, getBlocksByHeightMany) {
  const uint64_t NUMBER_OF_BLOCKS = 10;

  std::vector<uint64_t> blockHeights;
  std::vector<std::vector<CryptoNote::BlockDetails>> actualBlocks;

  std::vector<CryptoNote::Block> expectedBlocks;

  coreStub.set_blockchain_top(0, boost::value_initialized<crypto::hash>(), true);

  generator.generateEmptyBlocks(NUMBER_OF_BLOCKS);
  ASSERT_GE(generator.getBlockchain().size(), NUMBER_OF_BLOCKS);

  for (auto iter = generator.getBlockchain().begin() + 1; iter != generator.getBlockchain().end(); iter++) {
    expectedBlocks.push_back(*iter);
    blockHeights.push_back(std::move(boost::get<CryptoNote::TransactionInputGenerate>(iter->minerTx.vin.front()).height));
    coreStub.addBlock(*iter);
  }

  ASSERT_GE(blockHeights.size(), NUMBER_OF_BLOCKS);
  ASSERT_EQ(blockHeights.size(), expectedBlocks.size());
  ASSERT_EQ(actualBlocks.size(), 0);

  CallbackStatus status;
  node.getBlocks(blockHeights, actualBlocks, [&status] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.wait());
  ASSERT_EQ(std::error_code(), status.getStatus());

  ASSERT_EQ(blockHeights.size(), expectedBlocks.size());
  ASSERT_EQ(blockHeights.size(), actualBlocks.size());
  auto range1 = boost::combine(blockHeights, expectedBlocks);
  auto range = boost::combine(range1, actualBlocks);
  for (const boost::tuple<boost::tuple<size_t, CryptoNote::Block>, std::vector<CryptoNote::BlockDetails>>& sameHeight : range) {
    EXPECT_EQ(sameHeight.get<1>().size(), 1);
    for (const CryptoNote::BlockDetails& block : sameHeight.get<1>()) {
      EXPECT_EQ(block.height, sameHeight.get<0>().get<0>());
      crypto::hash expectedCryptoHash = CryptoNote::get_block_hash(sameHeight.get<0>().get<1>());
      std::array<uint8_t, 32> expectedHash = reinterpret_cast<const std::array<uint8_t, 32>&>(expectedCryptoHash);
      EXPECT_EQ(block.hash, expectedHash);
      EXPECT_FALSE(block.isOrphaned);
    }
  }
}

TEST_F(InProcessNode, getBlocksByHeightFail) {
  const uint64_t NUMBER_OF_BLOCKS = 10;

  std::vector<uint64_t> blockHeights;
  std::vector<std::vector<CryptoNote::BlockDetails>> actualBlocks;

  coreStub.set_blockchain_top(0, boost::value_initialized<crypto::hash>(), true);

  generator.generateEmptyBlocks(NUMBER_OF_BLOCKS);
  ASSERT_LT(generator.getBlockchain().size(), NUMBER_OF_BLOCKS * 2);

  for (const CryptoNote::Block& block : generator.getBlockchain()) {
    coreStub.addBlock(block);
  }

  for (uint64_t i = 0; i < NUMBER_OF_BLOCKS * 2; ++i) {
    blockHeights.push_back(std::move(i));
  }

  ASSERT_EQ(actualBlocks.size(), 0);

  CallbackStatus status;
  node.getBlocks(blockHeights, actualBlocks, [&status] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.wait());
  ASSERT_NE(std::error_code(), status.getStatus());
}

TEST_F(InProcessNode, getBlocksByHeightNotInited) {
  CryptoNote::InProcessNode newNode(coreStub, protocolQueryStub);

  std::vector<uint64_t> blockHeights;
  std::vector<std::vector<CryptoNote::BlockDetails>> blocks;
  ASSERT_EQ(blockHeights.size(), 0);
  ASSERT_EQ(blocks.size(), 0);

  CallbackStatus status;
  newNode.getBlocks(blockHeights, blocks, [&status] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.wait());
  ASSERT_NE(std::error_code(), status.getStatus());
}

TEST_F(InProcessNode, getBlocksByHashEmpty) {
  std::vector<crypto::hash> blockHashes;
  std::vector<CryptoNote::BlockDetails> blocks;
  ASSERT_EQ(blockHashes.size(), 0);
  ASSERT_EQ(blocks.size(), 0);

  coreStub.set_blockchain_top(0, boost::value_initialized<crypto::hash>(), true);

  CallbackStatus status;
  node.getBlocks(blockHashes, blocks, [&status] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.wait());
  ASSERT_EQ(std::error_code(), status.getStatus());
}

TEST_F(InProcessNode, getBlocksByHashMany) {
  const uint64_t NUMBER_OF_BLOCKS = 10;

  std::vector<crypto::hash> blockHashes;
  std::vector<CryptoNote::BlockDetails> actualBlocks;

  std::vector<CryptoNote::Block> expectedBlocks;

  coreStub.set_blockchain_top(0, boost::value_initialized<crypto::hash>(), true);

  generator.generateEmptyBlocks(NUMBER_OF_BLOCKS);
  ASSERT_GE(generator.getBlockchain().size(), NUMBER_OF_BLOCKS);

  for (auto iter = generator.getBlockchain().begin() + 1; iter != generator.getBlockchain().end(); iter++) {
    expectedBlocks.push_back(*iter);
    blockHashes.push_back(CryptoNote::get_block_hash(*iter));
    coreStub.addBlock(*iter);
  }

  ASSERT_GE(blockHashes.size(), NUMBER_OF_BLOCKS);
  ASSERT_EQ(blockHashes.size(), expectedBlocks.size());
  ASSERT_EQ(actualBlocks.size(), 0);

  CallbackStatus status;
  node.getBlocks(blockHashes, actualBlocks, [&status] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.wait());
  ASSERT_EQ(std::error_code(), status.getStatus());

  ASSERT_EQ(blockHashes.size(), expectedBlocks.size());
  ASSERT_EQ(blockHashes.size(), actualBlocks.size());
  auto range1 = boost::combine(blockHashes, expectedBlocks);
  auto range = boost::combine(range1, actualBlocks);
  for (const boost::tuple<boost::tuple<crypto::hash, CryptoNote::Block>, CryptoNote::BlockDetails>& sameHeight : range) {
    crypto::hash expectedCryptoHash = CryptoNote::get_block_hash(sameHeight.get<0>().get<1>());
    EXPECT_EQ(expectedCryptoHash, sameHeight.get<0>().get<0>());
    std::array<uint8_t, 32> expectedHash = reinterpret_cast<const std::array<uint8_t, 32>&>(expectedCryptoHash);
    EXPECT_EQ(sameHeight.get<1>().hash, expectedHash);
    EXPECT_FALSE(sameHeight.get<1>().isOrphaned);
  }
}

TEST_F(InProcessNode, getBlocksByHashFail) {
  const uint64_t NUMBER_OF_BLOCKS = 10;

  std::vector<crypto::hash> blockHashes;
  std::vector<CryptoNote::BlockDetails> actualBlocks;

  coreStub.set_blockchain_top(0, boost::value_initialized<crypto::hash>(), true);

  generator.generateEmptyBlocks(NUMBER_OF_BLOCKS);
  ASSERT_LT(generator.getBlockchain().size(), NUMBER_OF_BLOCKS * 2);

  for (const CryptoNote::Block& block : generator.getBlockchain()) {
    coreStub.addBlock(block);
  }

  for (uint64_t i = 0; i < NUMBER_OF_BLOCKS * 2; ++i) {
    blockHashes.push_back(boost::value_initialized<crypto::hash>());
  }

  ASSERT_EQ(actualBlocks.size(), 0);

  CallbackStatus status;
  node.getBlocks(blockHashes, actualBlocks, [&status] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.wait());
  ASSERT_NE(std::error_code(), status.getStatus());
}

TEST_F(InProcessNode, getBlocksByHashNotInited) {
  CryptoNote::InProcessNode newNode(coreStub, protocolQueryStub);

  std::vector<crypto::hash> blockHashes;
  std::vector<CryptoNote::BlockDetails> blocks;
  ASSERT_EQ(blockHashes.size(), 0);
  ASSERT_EQ(blocks.size(), 0);

  CallbackStatus status;
  newNode.getBlocks(blockHashes, blocks, [&status] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.wait());
  ASSERT_NE(std::error_code(), status.getStatus());
}

TEST_F(InProcessNode, getTxEmpty) {
  std::vector<crypto::hash> transactionHashes;
  std::vector<CryptoNote::TransactionDetails> transactions;
  ASSERT_EQ(transactionHashes.size(), 0);
  ASSERT_EQ(transactions.size(), 0);

  coreStub.set_blockchain_top(0, boost::value_initialized<crypto::hash>(), true);

  CallbackStatus status;
  node.getTransactions(transactionHashes, transactions, [&status] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.wait());
  ASSERT_EQ(std::error_code(), status.getStatus());
}

TEST_F(InProcessNode, getTxMany) {
  size_t POOL_TX_NUMBER = 10;
  size_t BLOCKCHAIN_TX_NUMBER = 10;

  std::vector<crypto::hash> transactionHashes;
  std::vector<CryptoNote::TransactionDetails> actualTransactions;

  std::vector<std::tuple<CryptoNote::Transaction, crypto::hash, uint64_t>> expectedTransactions;

  coreStub.set_blockchain_top(0, boost::value_initialized<crypto::hash>(), true);

  size_t prevBlockchainSize = generator.getBlockchain().size();
  for (size_t i = 0; i < BLOCKCHAIN_TX_NUMBER; ++i) {
    auto txptr = CryptoNote::createTransaction();
    auto tx = ::createTx(*txptr.get());
    transactionHashes.push_back(CryptoNote::get_transaction_hash(tx));
    generator.addTxToBlockchain(tx);
    ASSERT_EQ(generator.getBlockchain().size(), prevBlockchainSize + 1);
    prevBlockchainSize = generator.getBlockchain().size();
    coreStub.addBlock(generator.getBlockchain().back());
    coreStub.addTransaction(tx);
    expectedTransactions.push_back(std::make_tuple(tx, CryptoNote::get_block_hash(generator.getBlockchain().back()), boost::get<CryptoNote::TransactionInputGenerate>(generator.getBlockchain().back().minerTx.vin.front()).height));
  }

  ASSERT_EQ(transactionHashes.size(), BLOCKCHAIN_TX_NUMBER);
  ASSERT_EQ(transactionHashes.size(), expectedTransactions.size());
  ASSERT_EQ(actualTransactions.size(), 0);

  for (size_t i = 0; i < POOL_TX_NUMBER; ++i) {
    auto txptr = CryptoNote::createTransaction();
    auto tx = ::createTx(*txptr.get());
    transactionHashes.push_back(CryptoNote::get_transaction_hash(tx));
    coreStub.addTransaction(tx);
    expectedTransactions.push_back(std::make_tuple(tx, boost::value_initialized<crypto::hash>(), boost::value_initialized<uint64_t>()));
  }

  ASSERT_EQ(transactionHashes.size(), BLOCKCHAIN_TX_NUMBER + POOL_TX_NUMBER);
  ASSERT_EQ(transactionHashes.size(), expectedTransactions.size());
  ASSERT_EQ(actualTransactions.size(), 0);


  CallbackStatus status;
  node.getTransactions(transactionHashes, actualTransactions, [&status] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.wait());
  ASSERT_EQ(std::error_code(), status.getStatus());

  ASSERT_EQ(transactionHashes.size(), expectedTransactions.size());
  ASSERT_EQ(transactionHashes.size(), actualTransactions.size());
  auto range1 = boost::combine(transactionHashes, actualTransactions);
  auto range = boost::combine(range1, expectedTransactions);
  for (const boost::tuple<boost::tuple<crypto::hash, CryptoNote::TransactionDetails>, std::tuple<CryptoNote::Transaction, crypto::hash, uint64_t>>& sameHeight : range) {
    crypto::hash expectedCryptoHash = CryptoNote::get_transaction_hash(std::get<0>(sameHeight.get<1>()));
    EXPECT_EQ(expectedCryptoHash, sameHeight.get<0>().get<0>());
    std::array<uint8_t, 32> expectedHash = reinterpret_cast<const std::array<uint8_t, 32>&>(expectedCryptoHash);
    EXPECT_EQ(sameHeight.get<0>().get<1>().hash, expectedHash);
    if (std::get<1>(sameHeight.get<1>()) != boost::value_initialized<crypto::hash>()) {
      EXPECT_TRUE(sameHeight.get<0>().get<1>().inBlockchain);
      std::array<uint8_t, 32> expectedBlockHash = reinterpret_cast<const std::array<uint8_t, 32>&>(std::get<1>(sameHeight.get<1>()));
      EXPECT_EQ(sameHeight.get<0>().get<1>().blockHash, expectedBlockHash);
      EXPECT_EQ(sameHeight.get<0>().get<1>().blockHeight, std::get<2>(sameHeight.get<1>()));
    } else {
      EXPECT_FALSE(sameHeight.get<0>().get<1>().inBlockchain);
    }
  }
}

TEST_F(InProcessNode, getTxFail) {
size_t POOL_TX_NUMBER = 10;
  size_t BLOCKCHAIN_TX_NUMBER = 10;

  std::vector<crypto::hash> transactionHashes;
  std::vector<CryptoNote::TransactionDetails> actualTransactions;

  std::vector<std::tuple<CryptoNote::Transaction, crypto::hash, uint64_t>> expectedTransactions;

  coreStub.set_blockchain_top(0, boost::value_initialized<crypto::hash>(), true);

  size_t prevBlockchainSize = generator.getBlockchain().size();
  for (size_t i = 0; i < BLOCKCHAIN_TX_NUMBER; ++i) {
    auto txptr = CryptoNote::createTransaction();
    auto tx = ::createTx(*txptr.get());
    transactionHashes.push_back(CryptoNote::get_transaction_hash(tx));
    generator.addTxToBlockchain(tx);
    ASSERT_EQ(generator.getBlockchain().size(), prevBlockchainSize + 1);
    prevBlockchainSize = generator.getBlockchain().size();
    coreStub.addBlock(generator.getBlockchain().back());
    coreStub.addTransaction(tx);
    expectedTransactions.push_back(std::make_tuple(tx, CryptoNote::get_block_hash(generator.getBlockchain().back()), boost::get<CryptoNote::TransactionInputGenerate>(generator.getBlockchain().back().minerTx.vin.front()).height));
  }

  ASSERT_EQ(transactionHashes.size(), BLOCKCHAIN_TX_NUMBER);
  ASSERT_EQ(transactionHashes.size(), expectedTransactions.size());
  ASSERT_EQ(actualTransactions.size(), 0);

  for (size_t i = 0; i < POOL_TX_NUMBER; ++i) {
    auto txptr = CryptoNote::createTransaction();
    auto tx = ::createTx(*txptr.get());
    transactionHashes.push_back(CryptoNote::get_transaction_hash(tx));
    expectedTransactions.push_back(std::make_tuple(tx, boost::value_initialized<crypto::hash>(), boost::value_initialized<uint64_t>()));
  }

  ASSERT_EQ(transactionHashes.size(), BLOCKCHAIN_TX_NUMBER + POOL_TX_NUMBER);
  ASSERT_EQ(transactionHashes.size(), expectedTransactions.size());
  ASSERT_EQ(actualTransactions.size(), 0);


  CallbackStatus status;
  node.getTransactions(transactionHashes, actualTransactions, [&status] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.wait());
  ASSERT_NE(std::error_code(), status.getStatus());

}

TEST_F(InProcessNode, getTxNotInited) {
  CryptoNote::InProcessNode newNode(coreStub, protocolQueryStub);

  std::vector<crypto::hash> transactionHashes;
  std::vector<CryptoNote::TransactionDetails> transactions;
  ASSERT_EQ(transactionHashes.size(), 0);
  ASSERT_EQ(transactions.size(), 0);

  coreStub.set_blockchain_top(0, boost::value_initialized<crypto::hash>(), true);

  CallbackStatus status;
  newNode.getTransactions(transactionHashes, transactions, [&status] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.wait());
  ASSERT_NE(std::error_code(), status.getStatus());
}

TEST_F(InProcessNode, isSynchronized) {
  bool syncStatus;
  {
    CallbackStatus status;
    node.isSynchronized(syncStatus, [&status] (std::error_code ec) { status.setStatus(ec); });
    ASSERT_TRUE(status.wait());
    ASSERT_EQ(std::error_code(), status.getStatus());
    ASSERT_FALSE(syncStatus);
  }

  protocolQueryStub.setSynchronizedStatus(true);

  {
    CallbackStatus status;
    node.isSynchronized(syncStatus, [&status] (std::error_code ec) { status.setStatus(ec); });
    ASSERT_TRUE(status.wait());
    ASSERT_EQ(std::error_code(), status.getStatus());
    ASSERT_TRUE(syncStatus);
  }
}

TEST_F(InProcessNode, isSynchronizedNotInited) {
  CryptoNote::InProcessNode newNode(coreStub, protocolQueryStub);
  bool syncStatus;

  CallbackStatus status;
  newNode.isSynchronized(syncStatus, [&status] (std::error_code ec) { status.setStatus(ec); });
  ASSERT_TRUE(status.wait());
  ASSERT_NE(std::error_code(), status.getStatus());
}

//TODO: make relayTransaction unit test
//TODO: make getNewBlocks unit test
//TODO: make queryBlocks unit test
