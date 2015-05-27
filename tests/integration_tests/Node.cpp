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

#include <gtest/gtest.h>

#include <fstream>
#include <future>

#include <Logging/ConsoleLogger.h>

#include <System/Dispatcher.h>
#include <System/Timer.h>

#include <serialization/JsonOutputStreamSerializer.h>
#include <serialization/JsonInputStreamSerializer.h>
#include <serialization/SerializationOverloads.h>

#include "cryptonote_core/Currency.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "wallet/MultiWallet.h"

#include "../integration_test_lib/TestNetwork.h"
#include "../integration_test_lib/NodeObserver.h"

#include "BlockchainInfo.h"

using namespace Tests;
using namespace CryptoNote;

extern System::Dispatcher globalDispatcher;

class NodeCallback {
public:

  INode::Callback callback() {
    prom = std::promise<std::error_code>(); // reset std::promise
    return [this](std::error_code ec) {
      prom.set_value(ec);
    };
  }

  std::error_code get() {
    return prom.get_future().get();
  }

private:
  std::promise<std::error_code> prom;
};


class NodeTest: public testing::Test {
public:

  NodeTest() : 
    currency(CryptoNote::CurrencyBuilder(logger).testnet(true).currency()), 
    network(globalDispatcher, currency) {
  }

protected:

  virtual void TearDown() override {
    network.shutdown();
  }

  void startNetworkWithBlockchain(const std::string& sourcePath, size_t nodes = 2);
  void readBlockchainInfo(INode& node, BlockchainInfo& bc);
  void dumpBlockchainInfo(INode& node);

  Logging::ConsoleLogger logger;
  CryptoNote::Currency currency;
  TestNetwork network;
};

void NodeTest::startNetworkWithBlockchain(const std::string& sourcePath, size_t nodes) {
  auto networkCfg = TestNetworkBuilder(nodes, Topology::Ring).build();

  for (auto& node : networkCfg) {
    node.blockchainLocation = sourcePath;
  }

  network.addNodes(networkCfg);
  network.waitNodesReady();
}

void NodeTest::readBlockchainInfo(INode& node, BlockchainInfo& bc) {
  
  std::vector<crypto::hash> history = { currency.genesisBlockHash() };
  uint64_t timestamp = 0;
  uint64_t startHeight = 0;
  size_t itemsAdded = 0;
  NodeCallback cb;

  bc.blocks = { 
    BlockCompleteEntry{ currency.genesisBlockHash(), block_to_blob(currency.genesisBlock()) }
  };

  do {
    itemsAdded = 0;
    std::list<BlockCompleteEntry> blocks;
    node.queryBlocks(std::list<crypto::hash>(history.rbegin(), history.rend()), timestamp, blocks, startHeight, cb.callback());

    ASSERT_TRUE(cb.get() == std::error_code());

    uint64_t currentHeight = startHeight;

    for (auto& entry : blocks) {

      if (currentHeight < history.size()) {
        // detach no expected
        ASSERT_EQ(entry.blockHash, history[currentHeight]);
      } else {

        CryptoNote::Block block;
        CryptoNote::parse_and_validate_block_from_blob(entry.block, block);

        auto txHash = get_transaction_hash(block.minerTx);

        std::vector<uint64_t> globalIndices;
        node.getTransactionOutsGlobalIndices(txHash, globalIndices, cb.callback());

        ASSERT_TRUE(!cb.get());

        bc.globalOutputs.insert(std::make_pair(txHash, std::move(globalIndices)));

        bc.blocks.push_back(entry);
        history.push_back(entry.blockHash);
        ++itemsAdded;
      }

      ++currentHeight;
    }
  } while (itemsAdded > 0);
}

void NodeTest::dumpBlockchainInfo(INode& node) {
  BlockchainInfo bc;
  ASSERT_NO_FATAL_FAILURE(readBlockchainInfo(node, bc));
  storeBlockchainInfo("blocks.js", bc);
}


//TEST_F(NodeTest, generateBlockchain) {
//  
//  auto networkCfg = TestNetworkBuilder(2, Topology::Ring).build();
//  networkCfg[0].cleanupDataDir = false;
//  network.addNodes(networkCfg);
//  network.waitNodesReady();
//
//  auto& daemon = network.getNode(0);
//
//  {
//    std::unique_ptr<INode> mainNode;
//    ASSERT_TRUE(daemon.makeINode(mainNode));
//
//    std::string password = "pass";
//    CryptoNote::MultiWallet wallet(globalDispatcher, currency, *mainNode);
//
//    wallet.initialize(password);
//
//    std::string minerAddress = wallet.createAddress();
//    daemon.startMining(1, minerAddress);
//
//    System::Timer timer(globalDispatcher);
//
//    while (daemon.getLocalHeight() < 300) {
//      std::cout << "Waiting for block..." << std::endl;
//      timer.sleep(std::chrono::seconds(10));
//    }
//
//    daemon.stopMining();
//
//    std::ofstream walletFile("wallet.bin", std::ios::binary | std::ios::trunc);
//    wallet.save(walletFile);
//    wallet.shutdown();
//
//    dumpBlockchainInfo(*mainNode);
// }
//}
//
//
//TEST_F(NodeTest, addMoreBlocks) {
//  auto networkCfg = TestNetworkBuilder(2, Topology::Ring).build();
//  networkCfg[0].cleanupDataDir = false;
//  networkCfg[0].blockchainLocation = "testnet_300";
//  networkCfg[1].blockchainLocation = "testnet_300";
//  network.addNodes(networkCfg);
//  network.waitNodesReady();
//
//  auto& daemon = network.getNode(0);
//
//  {
//    std::unique_ptr<INode> mainNode;
//    ASSERT_TRUE(daemon.makeINode(mainNode));
//
//    auto startHeight = daemon.getLocalHeight();
//
//    std::string password = "pass";
//    CryptoNote::MultiWallet wallet(globalDispatcher, currency, *mainNode);
//
//    {
//      std::ifstream walletFile("wallet.bin", std::ios::binary);
//      wallet.load(walletFile, password);
//    }
//
//    std::string minerAddress = wallet.getAddress(0);
//    daemon.startMining(1, minerAddress);
//
//    System::Timer timer(globalDispatcher);
//
//    while (daemon.getLocalHeight() <= startHeight + 3) {
//      std::cout << "Waiting for block..." << std::endl;
//      timer.sleep(std::chrono::seconds(1));
//    }
//
//    daemon.stopMining();
//
//    std::ofstream walletFile("wallet.bin", std::ios::binary | std::ios::trunc);
//    wallet.save(walletFile);
//    wallet.shutdown();
//
//    dumpBlockchainInfo(*mainNode);
//  }
//}

//TEST_F(NodeTest, dumpBlockchain) {
//  startNetworkWithBlockchain("testnet_300", 2);
//  auto& daemon = network.getNode(0);
//  std::unique_ptr<INode> mainNode;
//  ASSERT_TRUE(daemon.makeINode(mainNode));
//  dumpBlockchainInfo(*mainNode);
//}


class QueryBlocksTest : public NodeTest {
public:

  virtual void SetUp() override {
    NodeTest::SetUp();

    loadBlockchainInfo("blocks.js", knownBc);

    startNetworkWithBlockchain("testnet_300", 2);
    auto& daemon = network.getNode(0);
    // check full sync
    ASSERT_TRUE(daemon.makeINode(mainNode));
  }

  virtual void TearDown() override {
    mainNode.reset();
    NodeTest::TearDown();
  }

  BlockchainInfo knownBc;
  std::unique_ptr<INode> mainNode;
};

TEST_F(QueryBlocksTest, fullSync) {
  BlockchainInfo nodeBc;
  ASSERT_NO_FATAL_FAILURE(readBlockchainInfo(*mainNode, nodeBc));
  ASSERT_EQ(knownBc, nodeBc);
}

TEST_F(QueryBlocksTest, queryByTimestamp) {
  size_t pivotBlockIndex = knownBc.blocks.size() / 3 * 2;
  Block block;

  auto iter = knownBc.blocks.begin();
  std::advance(iter, pivotBlockIndex);

  parse_and_validate_block_from_blob(iter->block, block);
  auto timestamp = block.timestamp - 1;
  uint64_t startHeight = 0;
  std::list<BlockCompleteEntry> blocks;

  std::cout << "Requesting timestamp: " << timestamp << std::endl;

  NodeCallback cb;

  std::list<crypto::hash> history = { currency.genesisBlockHash() };

  mainNode->queryBlocks(std::list<crypto::hash>(history), timestamp, blocks, startHeight, cb.callback());
  ASSERT_TRUE(!cb.get());

  EXPECT_EQ(0, startHeight);
  EXPECT_EQ(knownBc.blocks.begin()->blockHash, blocks.begin()->blockHash);
  EXPECT_EQ(knownBc.blocks.size(), blocks.size());

  auto startBlockIter = std::find_if(blocks.begin(), blocks.end(), [](const BlockCompleteEntry& e) { return !e.block.empty(); });
  ASSERT_TRUE(startBlockIter != blocks.end());

  Block startBlock;
  ASSERT_TRUE(parse_and_validate_block_from_blob(startBlockIter->block, startBlock));

  std::cout << "Starting block timestamp: " << startBlock.timestamp << std::endl;
  auto startFullIndex = std::distance(blocks.begin(), startBlockIter);

  auto it = blocks.begin();
  for (const auto& item : knownBc.blocks) {
    ASSERT_EQ(item.blockHash, it->blockHash);
    ++it;
  }

  ASSERT_EQ(pivotBlockIndex, startFullIndex);
}

TEST_F(QueryBlocksTest, queryHistory) {
  NodeCallback cb;
  uint64_t startHeight = 0;
  std::list<BlockCompleteEntry> blocks;

  // random genesis block hash -> error
  auto randomHash = crypto::rand<crypto::hash>();
  mainNode->queryBlocks({ randomHash }, 0, blocks, startHeight, cb.callback());
  ASSERT_FALSE(!cb.get());

  // unknown block - start from first known
  mainNode->queryBlocks({ randomHash, currency.genesisBlockHash() }, 0, blocks, startHeight, cb.callback());
  ASSERT_TRUE(!cb.get());
  ASSERT_EQ(0, startHeight);
  ASSERT_GT(blocks.size(), 1);
  ASSERT_EQ(currency.genesisBlockHash(), blocks.begin()->blockHash);

  for (size_t idx = 10; idx <= 100; idx += 10) {
    blocks.clear();
    startHeight = 0;

    const auto& knownBlockHash = knownBc.blocks[idx].blockHash;

    std::list<crypto::hash> history = { knownBlockHash, currency.genesisBlockHash() };
    mainNode->queryBlocks(std::list<crypto::hash>(history), 0, blocks, startHeight, cb.callback());
    
    ASSERT_TRUE(!cb.get());
    EXPECT_EQ(idx, startHeight);
    EXPECT_EQ(knownBlockHash, blocks.begin()->blockHash);
  }
}


TEST_F(NodeTest, queryBlocks) {
  BlockchainInfo knownBc, nodeBc;

  loadBlockchainInfo("blocks.js", knownBc);

  startNetworkWithBlockchain("testnet_300", 2);
  auto& daemon = network.getNode(0);
  std::unique_ptr<INode> mainNode;

  // check full sync

  ASSERT_TRUE(daemon.makeINode(mainNode));
  ASSERT_NO_FATAL_FAILURE(readBlockchainInfo(*mainNode, nodeBc));
  ASSERT_EQ(knownBc, nodeBc);

  // check query with timestamp

  size_t pivotBlockIndex = knownBc.blocks.size() / 3 * 2;
  Block block;

  auto iter = knownBc.blocks.begin();
  std::advance(iter, pivotBlockIndex);

  parse_and_validate_block_from_blob(iter->block, block);
  auto timestamp = block.timestamp - 1;
  uint64_t startHeight = 0;
  std::list<BlockCompleteEntry> blocks;

  std::cout << "Requesting timestamp: " << timestamp << std::endl;
  
  NodeCallback cb;

  std::list<crypto::hash> history = { currency.genesisBlockHash() };

  mainNode->queryBlocks(std::list<crypto::hash>(history), timestamp, blocks, startHeight, cb.callback());
  ASSERT_TRUE(!cb.get());

  EXPECT_EQ(0, startHeight);
  EXPECT_EQ(knownBc.blocks.begin()->blockHash, blocks.begin()->blockHash);
  EXPECT_EQ(knownBc.blocks.size(), blocks.size());

  auto startBlockIter = std::find_if(blocks.begin(), blocks.end(), [](const BlockCompleteEntry& e) { return !e.block.empty(); });
  ASSERT_TRUE(startBlockIter != blocks.end());

  Block startBlock;
  ASSERT_TRUE(parse_and_validate_block_from_blob(startBlockIter->block, startBlock));

  std::cout << "Starting block timestamp: " << startBlock.timestamp << std::endl;
  auto startFullIndex = std::distance(blocks.begin(), startBlockIter);

  auto it = blocks.begin();
  for (const auto& item : knownBc.blocks) {
    ASSERT_EQ(item.blockHash, it->blockHash);
    ++it;
  }

  ASSERT_EQ(pivotBlockIndex, startFullIndex);
}


TEST_F(NodeTest, observerHeightNotifications) {
  BlockchainInfo extraBlocks;
  loadBlockchainInfo("blocks_extra.js", extraBlocks);

  startNetworkWithBlockchain("testnet_300");

  auto& daemon = network.getNode(0);

  {
    std::unique_ptr<INode> mainNode;
    daemon.makeINode(mainNode);

    NodeObserver observer(*mainNode);

    std::chrono::seconds timeout(10);
    uint64_t knownHeight = 0;
    uint64_t localHeight = 0;
    size_t peerCount = 0;

    EXPECT_TRUE(observer.m_localHeight.waitFor(timeout, localHeight));
    EXPECT_TRUE(observer.m_knownHeight.waitFor(timeout, knownHeight));
    EXPECT_TRUE(observer.m_peerCount.waitFor(timeout, peerCount));

    EXPECT_GT(localHeight, 0);
    EXPECT_GT(knownHeight, 0);
    EXPECT_GT(peerCount, 0);

    std::cout << "Local height = " << localHeight << std::endl;
    std::cout << "Known height = " << knownHeight << std::endl;
    std::cout << "Peer count = " << peerCount << std::endl;

    EXPECT_EQ(localHeight, mainNode->getLastLocalBlockHeight());
    EXPECT_EQ(knownHeight, mainNode->getLastKnownBlockHeight());

    // submit 1 block and check observer

    uint64_t newKnownHeight = 0;
    uint64_t newLocalHeight = 0;

    auto blockData = extraBlocks.blocks.begin()->block;
    ASSERT_TRUE(daemon.submitBlock(Common::toHex(blockData.data(), blockData.size())));

    ASSERT_TRUE(observer.m_localHeight.waitFor(timeout, newLocalHeight));
    ASSERT_TRUE(observer.m_knownHeight.waitFor(timeout, newKnownHeight));

    size_t blocksSubmitted = 1;

    EXPECT_EQ(localHeight + blocksSubmitted, newLocalHeight);
    EXPECT_EQ(knownHeight + blocksSubmitted, newKnownHeight);

    EXPECT_EQ(newLocalHeight, mainNode->getLastLocalBlockHeight());
    EXPECT_EQ(newKnownHeight, mainNode->getLastKnownBlockHeight());

    std::cout << "Local height = " << newLocalHeight << std::endl;
    std::cout << "Known height = " << newKnownHeight << std::endl;
  }
}
