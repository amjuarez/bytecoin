// Copyright (c) 2012-2014, The CryptoNote developers, The Bytecoin developers
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

#include "transfers/BlockchainSynchronizer.h"
#include "transfers/TransfersConsumer.h"

#include "cryptonote_core/TransactionApi.h"
#include "cryptonote_core/cryptonote_format_utils.h"

#include "INodeStubs.h"
#include "TestBlockchainGenerator.h"
#include "EventWaiter.h"

using namespace CryptoNote;

namespace {
cryptonote::Transaction createTx(ITransactionReader& tx) {
  auto data = tx.getTransactionData();

  cryptonote::blobdata txblob(data.data(), data.data() + data.size());
  cryptonote::Transaction outTx;
  cryptonote::parse_and_validate_tx_from_blob(txblob, outTx);

  return outTx;
}
}

class INodeNonTrivialRefreshStub : public INodeTrivialRefreshStub {
public:

  INodeNonTrivialRefreshStub(TestBlockchainGenerator& generator) : INodeTrivialRefreshStub(generator), blocksWasQueried(false), poolWasQueried(false) {}

  virtual void queryBlocks(std::list<crypto::hash>&& knownBlockIds, uint64_t timestamp, std::list<CryptoNote::BlockCompleteEntry>& newBlocks, uint64_t& startHeight, const Callback& callback) override {
    blocksWasQueried = true;
    INodeTrivialRefreshStub::queryBlocks(std::move(knownBlockIds), timestamp, newBlocks, startHeight, callback);
  }

  virtual void getPoolSymmetricDifference(std::vector<crypto::hash>&& known_pool_tx_ids, crypto::hash known_block_id, bool& is_bc_actual,
    std::vector<cryptonote::Transaction>& new_txs, std::vector<crypto::hash>& deleted_tx_ids, const Callback& callback) override {
    poolWasQueried = true;
    INodeTrivialRefreshStub::getPoolSymmetricDifference(std::move(known_pool_tx_ids), known_block_id, is_bc_actual, new_txs, deleted_tx_ids, callback);
  }

  void notifyAboutPool() {
    observerManager.notify(&CryptoNote::INodeObserver::poolChanged);
  }

  bool blocksWasQueried;
  bool poolWasQueried;

};

class INodeFunctorialStub : public INodeNonTrivialRefreshStub {
public:

  INodeFunctorialStub(TestBlockchainGenerator& generator)
    : INodeNonTrivialRefreshStub(generator)
    , queryBlocksFunctor([](const std::list<crypto::hash>&, uint64_t, std::list<CryptoNote::BlockCompleteEntry>&, uint64_t&, const Callback&)->bool {return true; })
    , getPoolSymmetricDifferenceFunctor([](const std::vector<crypto::hash>&, crypto::hash, bool&, std::vector<cryptonote::Transaction>&, std::vector<crypto::hash>&, const Callback&)->bool {return true; }) {
  }

  virtual void queryBlocks(std::list<crypto::hash>&& knownBlockIds, uint64_t timestamp, std::list<CryptoNote::BlockCompleteEntry>& newBlocks, uint64_t& startHeight, const Callback& callback) override {
    if (queryBlocksFunctor(knownBlockIds, timestamp, newBlocks, startHeight, callback)) {
      INodeNonTrivialRefreshStub::queryBlocks(std::move(knownBlockIds), timestamp, newBlocks, startHeight, callback);
    }
  }

  virtual void getPoolSymmetricDifference(std::vector<crypto::hash>&& known_pool_tx_ids, crypto::hash known_block_id, bool& is_bc_actual,
    std::vector<cryptonote::Transaction>& new_txs, std::vector<crypto::hash>& deleted_tx_ids, const Callback& callback) override {
    if (getPoolSymmetricDifferenceFunctor(known_pool_tx_ids, known_block_id, is_bc_actual, new_txs, deleted_tx_ids, callback)) {
      INodeNonTrivialRefreshStub::getPoolSymmetricDifference(std::move(known_pool_tx_ids), known_block_id, is_bc_actual, new_txs, deleted_tx_ids, callback);
    }
  }

  std::function<bool(const std::list<crypto::hash>&, uint64_t, std::list<CryptoNote::BlockCompleteEntry>&, uint64_t&, const Callback&)> queryBlocksFunctor;
  std::function<bool(const std::vector<crypto::hash>&, crypto::hash, bool&, std::vector<cryptonote::Transaction>&, std::vector<crypto::hash>&, const Callback&)> getPoolSymmetricDifferenceFunctor;

};

class IBlockchainSynchronizerTrivialObserver : public IBlockchainSynchronizerObserver {
public:
  virtual void synchronizationProgressUpdated(uint64_t current, uint64_t total) override { m_current = current; m_total = total; }
  virtual void synchronizationCompleted(std::error_code result) override { completionResult = result; }

  std::error_code completionResult;
  uint64_t m_current;
  uint64_t m_total;
};

class IBlockchainSynchronizerFunctorialObserver : public IBlockchainSynchronizerObserver {
public:
  IBlockchainSynchronizerFunctorialObserver() : updFunc([](uint64_t, uint64_t) {}), syncFunc([](std::error_code) {}) {
  }

  virtual void synchronizationProgressUpdated(uint64_t current, uint64_t total) override { updFunc(current, total); }
  virtual void synchronizationCompleted(std::error_code result) override { syncFunc(result); }

  std::function<void(uint64_t, uint64_t)> updFunc;
  std::function<void(std::error_code)> syncFunc;
};

class ConsumerStub : public IBlockchainConsumer {
public:
  ConsumerStub(const crypto::hash& genesisBlockHash) {
    m_blockchain.push_back(genesisBlockHash);
  }

  virtual SynchronizationStart getSyncStart() override {
    SynchronizationStart start = { 0, 0 };
    return start;
  }

  virtual void onBlockchainDetach(uint64_t height) override {
    assert(height < m_blockchain.size());
    m_blockchain.resize(height);
  }

  virtual bool onNewBlocks(const CompleteBlock* blocks, uint64_t startHeight, size_t count) override {
    //assert(m_blockchain.size() == startHeight);
    while (count--) {
      m_blockchain.push_back(blocks->blockHash);
      ++blocks;
    }
    return true;
  }

  const std::vector<crypto::hash>& getBlockchain() const {
    return m_blockchain;
  }

  virtual void getKnownPoolTxIds(std::vector<crypto::hash>& ids) override {
    ids.clear();
    for (auto& tx : m_pool) {
      ids.push_back(cryptonote::get_transaction_hash(tx));
    }
  }

  virtual std::error_code onPoolUpdated(const std::vector<cryptonote::Transaction>& addedTransactions, const std::vector<crypto::hash>& deletedTransactions) override {
    m_pool.insert(m_pool.end(), addedTransactions.begin(), addedTransactions.end());

    for (auto& hash : deletedTransactions) {
      auto pos = std::find_if(m_pool.begin(), m_pool.end(), [&hash](const cryptonote::Transaction& t)->bool { return hash == cryptonote::get_transaction_hash(t); });
      if (pos != m_pool.end()) {
        m_pool.erase(pos);
      }
    }

    return std::error_code();
  }

private:
  std::vector<cryptonote::Transaction> m_pool;
  std::vector<crypto::hash> m_blockchain;
};

class BcSTest : public ::testing::Test, public IBlockchainSynchronizerObserver {
public:
  BcSTest() :
    m_currency(cryptonote::CurrencyBuilder().currency()),
    generator(m_currency),
    m_node(generator),
    m_sync(m_node, m_currency.genesisBlockHash()) {
    m_node.setGetNewBlocksLimit(5); // sync max 5 blocks per request
  }

  void addConsumers(size_t count = 1) {
    while (count--) {
      std::shared_ptr<ConsumerStub> stub(new ConsumerStub(m_currency.genesisBlockHash()));
      m_sync.addConsumer(stub.get());
      m_consumers.push_back(stub);
    }
  }

  void checkSyncedBlockchains() {
    std::vector<crypto::hash> generatorBlockchain;
    std::transform(
      generator.getBlockchain().begin(),
      generator.getBlockchain().end(),
      std::back_inserter(generatorBlockchain),
      [](const cryptonote::Block& b) { return cryptonote::get_block_hash(b); });

    for (const auto& consumer : m_consumers) {
      ASSERT_EQ(consumer->getBlockchain(), generatorBlockchain);
    }
  }

  void startSync() {
    syncCompleted = std::promise<std::error_code>();
    syncCompletedFuture = syncCompleted.get_future();
    m_sync.addObserver(this);
    m_sync.start();
    syncCompletedFuture.get();
    m_sync.removeObserver(this);
  }

  void refreshSync() {
    syncCompleted = std::promise<std::error_code>();
    syncCompletedFuture = syncCompleted.get_future();
    m_sync.addObserver(this);
    m_node.updateObservers();
    syncCompletedFuture.get();
    m_sync.removeObserver(this);
  }

  void synchronizationCompleted(std::error_code result) override {
    decltype(syncCompleted) detachedPromise = std::move(syncCompleted);
    detachedPromise.set_value(result);
  }

protected:
  cryptonote::Currency m_currency;
  TestBlockchainGenerator generator;

  INodeFunctorialStub m_node;
  BlockchainSynchronizer m_sync;
  std::vector<std::shared_ptr<ConsumerStub>> m_consumers;

  std::promise<std::error_code> syncCompleted;
  std::future<std::error_code> syncCompletedFuture;
};

TEST_F(BcSTest, addConsumerStopped) {
  ASSERT_NO_THROW(addConsumers());
}

TEST_F(BcSTest, addConsumerStartStop) {
  addConsumers();
  m_sync.start();
  m_sync.stop();
  ASSERT_NO_THROW(addConsumers());
}

TEST_F(BcSTest, addConsumerStartThrow) {
  addConsumers();
  m_sync.start();
  ASSERT_ANY_THROW(addConsumers());
  m_sync.stop();
}

TEST_F(BcSTest, removeConsumerWhichIsNotExist) {
  ConsumerStub c(m_currency.genesisBlockHash());
  ASSERT_FALSE(m_sync.removeConsumer(&c));
}

TEST_F(BcSTest, removeConsumerStartThrow) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  m_sync.start();
  ASSERT_ANY_THROW(m_sync.removeConsumer(&c));
  m_sync.stop();
}

TEST_F(BcSTest, removeConsumerStopped) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  EXPECT_EQ(true, m_sync.removeConsumer(&c));
}

TEST_F(BcSTest, removeConsumerStartStop) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  m_sync.start();
  m_sync.stop();
  EXPECT_EQ(true, m_sync.removeConsumer(&c));
}

TEST_F(BcSTest, getConsumerStateWhichIsNotExist) {
  ConsumerStub c(m_currency.genesisBlockHash());
  EXPECT_EQ(nullptr, m_sync.getConsumerState(&c));
}

TEST_F(BcSTest, getConsumerStateStartThrow) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  m_sync.start();
  ASSERT_ANY_THROW(m_sync.getConsumerState(&c));
  m_sync.stop();
}

TEST_F(BcSTest, getConsumerStateStopped) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  EXPECT_NE(nullptr, m_sync.getConsumerState(&c));
}

TEST_F(BcSTest, getConsumerStateStartStop) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  m_sync.start();
  m_sync.stop();
  EXPECT_NE(nullptr, m_sync.getConsumerState(&c));
}

TEST_F(BcSTest, startWithoutConsumersThrow) {
  ASSERT_ANY_THROW(m_sync.start());
}

TEST_F(BcSTest, doubleStart) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  m_sync.start();
  ASSERT_ANY_THROW(m_sync.start());
  m_sync.stop();
}

TEST_F(BcSTest, startAfterStop) {
  addConsumers();
  m_sync.start();
  m_sync.stop();
  ASSERT_NO_THROW(m_sync.start());
  m_sync.stop();
}

TEST_F(BcSTest, startAndObserve) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  m_sync.start();
  ASSERT_ANY_THROW(m_sync.start());
  m_sync.stop();
}

TEST_F(BcSTest, noObservationsBeforeStart) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  m_node.updateObservers();
  ASSERT_FALSE(m_node.blocksWasQueried);
}

TEST_F(BcSTest, noObservationsAfterStop) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  m_sync.start();
  m_sync.stop();
  m_node.blocksWasQueried = false;
  m_node.updateObservers();
  ASSERT_FALSE(m_node.blocksWasQueried);
}

TEST_F(BcSTest, stopOnCreation) {
  ASSERT_NO_THROW(m_sync.stop());
}

TEST_F(BcSTest, doubleStopAfterStart) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  m_sync.start();
  m_sync.stop();
  ASSERT_NO_THROW(m_sync.stop());
}

TEST_F(BcSTest, stopIsWaiting) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  generator.generateEmptyBlocks(20);
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;

  bool flag = false;

  o1.updFunc = std::move([&e, &flag](uint64_t, uint64_t) {
    e.notify(); std::this_thread::sleep_for(std::chrono::milliseconds(1000)); flag = true;

  });

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(flag, true);
}

TEST_F(BcSTest, syncCompletedError) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  generator.generateEmptyBlocks(20);
  IBlockchainSynchronizerTrivialObserver o;
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;

  o1.updFunc = std::move([&e](uint64_t curr, uint64_t total) {
    e.notify(); std::this_thread::sleep_for(std::chrono::milliseconds(200));
  });

  m_sync.addObserver(&o);
  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(std::errc::interrupted, o.completionResult);
}

TEST_F(BcSTest, onLastKnownBlockHeightUpdated) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  generator.generateEmptyBlocks(20);
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  o1.syncFunc = std::move([&e](std::error_code) {
    e.notify();
  });

  m_sync.addObserver(&o1);
  m_sync.start();

  e.wait();
  m_node.blocksWasQueried = false;
  m_node.poolWasQueried = false;
  m_node.updateObservers();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(true, m_node.blocksWasQueried);
  EXPECT_EQ(true, m_node.poolWasQueried);
}

TEST_F(BcSTest, onPoolChanged) {
  ConsumerStub c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&c);
  generator.generateEmptyBlocks(20);
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  o1.syncFunc = std::move([&e](std::error_code) {
    e.notify();
  });

  m_sync.addObserver(&o1);
  m_sync.start();

  e.wait();
  m_node.poolWasQueried = false;
  m_node.notifyAboutPool();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(true, m_node.poolWasQueried);
}

TEST_F(BcSTest, serializationCheck) {
  addConsumers(2);

  std::stringstream memstream;
  m_sync.save(memstream);

  ASSERT_GT(memstream.str().size(), 0);

  std::string first = memstream.str();

  BlockchainSynchronizer sync2(m_node, m_currency.genesisBlockHash());

  ASSERT_NO_THROW(sync2.load(memstream));
  std::stringstream memstream2;
  m_sync.save(memstream2);
  EXPECT_EQ(memstream2.str(), first);
}

class FunctorialPoolConsumerStub : public ConsumerStub {
public:

  FunctorialPoolConsumerStub(const crypto::hash& genesisBlockHash) : ConsumerStub(genesisBlockHash) {}

  virtual void getKnownPoolTxIds(std::vector<crypto::hash>& ids) override {
    getKnownPoolTxIdsFunctor(ids);
  }

  virtual std::error_code onPoolUpdated(const std::vector<cryptonote::Transaction>& addedTransactions, const std::vector<crypto::hash>& deletedTransactions) override {
    return onPoolUpdatedFunctor(addedTransactions, deletedTransactions);
  }

  std::function<void(std::vector<crypto::hash>&)> getKnownPoolTxIdsFunctor;
  std::function<std::error_code(const std::vector<cryptonote::Transaction>&, const std::vector<crypto::hash>&)> onPoolUpdatedFunctor;
};

TEST_F(BcSTest, firstPoolSynchronizationCheck) {
  auto tx1ptr = CryptoNote::createTransaction();
  auto tx2ptr = CryptoNote::createTransaction();
  auto tx3ptr = CryptoNote::createTransaction();

  auto tx1 = ::createTx(*tx1ptr.get());
  auto tx2 = ::createTx(*tx2ptr.get());
  auto tx3 = ::createTx(*tx3ptr.get());

  auto tx1hash = cryptonote::get_transaction_hash(tx1);
  auto tx2hash = cryptonote::get_transaction_hash(tx2);
  auto tx3hash = cryptonote::get_transaction_hash(tx3);

  std::vector<crypto::hash> consumer1Pool = { tx1hash, tx2hash };
  std::vector<crypto::hash> consumer2Pool = { tx2hash, tx3hash };
  std::unordered_set<crypto::hash> firstExpectedPool = { tx1hash, tx2hash, tx3hash };
  std::unordered_set<crypto::hash> secondExpectedPool = { tx2hash };

  std::vector<crypto::hash> expectedDeletedPoolAnswer = { tx3hash };
  std::vector<cryptonote::Transaction> expectedNewPoolAnswer = { tx1 };

  FunctorialPoolConsumerStub c1(m_currency.genesisBlockHash());
  FunctorialPoolConsumerStub c2(m_currency.genesisBlockHash());

  c1.getKnownPoolTxIdsFunctor = [&](std::vector<crypto::hash>& ids) { ids.assign(consumer1Pool.begin(), consumer1Pool.end()); };
  c2.getKnownPoolTxIdsFunctor = [&](std::vector<crypto::hash>& ids) { ids.assign(consumer2Pool.begin(), consumer2Pool.end()); };

  std::vector<crypto::hash> c1ResponseDeletedPool;
  std::vector<crypto::hash> c2ResponseDeletedPool;
  std::vector<cryptonote::Transaction> c1ResponseNewPool;
  std::vector<cryptonote::Transaction> c2ResponseNewPool;


  c1.onPoolUpdatedFunctor = [&](const std::vector<cryptonote::Transaction>& new_txs, const std::vector<crypto::hash>& deleted)->std::error_code {
    c1ResponseDeletedPool.assign(deleted.begin(), deleted.end());
    c1ResponseNewPool.assign(new_txs.begin(), new_txs.end());
    return std::error_code();
  };

  c2.onPoolUpdatedFunctor = [&](const std::vector<cryptonote::Transaction>& new_txs, const std::vector<crypto::hash>& deleted)->std::error_code {
    c2ResponseDeletedPool.assign(deleted.begin(), deleted.end());
    c2ResponseNewPool.assign(new_txs.begin(), new_txs.end());
    return std::error_code();
  };

  m_sync.addConsumer(&c1);
  m_sync.addConsumer(&c2);

  int requestsCount = 0;
  std::unordered_set<crypto::hash> firstKnownPool;
  std::unordered_set<crypto::hash> secondKnownPool;


  m_node.getPoolSymmetricDifferenceFunctor = [&](const std::vector<crypto::hash>& known, crypto::hash last, bool& is_actual, std::vector<cryptonote::Transaction>& new_txs, std::vector<crypto::hash>& deleted, const INode::Callback& callback) {
    is_actual = true;
    requestsCount++;

    new_txs.assign(expectedNewPoolAnswer.begin(), expectedNewPoolAnswer.end());
    deleted.assign(expectedDeletedPoolAnswer.begin(), expectedDeletedPoolAnswer.end());

    if (requestsCount == 1) {
      firstKnownPool.insert(known.begin(), known.end());
    }

    if (requestsCount == 2) {
      secondKnownPool.insert(known.begin(), known.end());
    }

    callback(std::error_code());

    return false;
  };

  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  o1.syncFunc = std::move([&e](std::error_code) {
    e.notify();
  });

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(2, requestsCount);
  EXPECT_EQ(firstExpectedPool, firstKnownPool);
  EXPECT_EQ(secondExpectedPool, secondKnownPool);
  EXPECT_EQ(expectedDeletedPoolAnswer, c1ResponseDeletedPool);
  EXPECT_EQ(expectedDeletedPoolAnswer, c2ResponseDeletedPool);
  EXPECT_EQ(expectedNewPoolAnswer, c1ResponseNewPool);
  EXPECT_EQ(expectedNewPoolAnswer, c2ResponseNewPool);
}

TEST_F(BcSTest, firstPoolSynchronizationCheckNonActual) {
  addConsumers(2);

  int requestsCount = 0;

  m_node.getPoolSymmetricDifferenceFunctor = [&](const std::vector<crypto::hash>& known, crypto::hash last, bool& is_actual, std::vector<cryptonote::Transaction>& new_txs, std::vector<crypto::hash>& deleted, const INode::Callback& callback) {
    is_actual = true;
    requestsCount++;

    if (requestsCount == 2) {
      is_actual = false;
    }

    callback(std::error_code());
    return false;
  };

  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  o1.syncFunc = std::move([&e](std::error_code) {
    e.notify();
  });

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};


  EXPECT_EQ(4, requestsCount);
}

TEST_F(BcSTest, firstPoolSynchronizationCheckGetPoolErr) {
  addConsumers(2);

  int requestsCount = 0;

  m_node.getPoolSymmetricDifferenceFunctor = [&](const std::vector<crypto::hash>& known, crypto::hash last, bool& is_actual, std::vector<cryptonote::Transaction>& new_txs, std::vector<crypto::hash>& deleted, const INode::Callback& callback) {
    is_actual = true;
    requestsCount++;

    if (requestsCount == 2) {
      callback(std::make_error_code(std::errc::invalid_argument));
    } else {
      callback(std::error_code());
    }

    return false;
  };

  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  o1.syncFunc = std::move([&e](std::error_code) {
    e.notify();
  });

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();
  m_node.notifyAboutPool();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(4, requestsCount);
}

TEST_F(BcSTest, poolSynchronizationCheckActual) {
  addConsumers(1);

  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  o1.syncFunc = std::move([&e](std::error_code) {
    e.notify();
  });

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();

  int requestsCount = 0;

  m_node.getPoolSymmetricDifferenceFunctor = [&](const std::vector<crypto::hash>& known, crypto::hash last, bool& is_actual, std::vector<cryptonote::Transaction>& new_txs, std::vector<crypto::hash>& deleted, const INode::Callback& callback) {
    is_actual = true;
    requestsCount++;

    if (requestsCount == 1) {
      is_actual = false;
    }

    callback(std::error_code());
    return false;
  };

  m_node.notifyAboutPool();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(2, requestsCount);
}

TEST_F(BcSTest, poolSynchronizationCheckError) {
  addConsumers(1);

  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = std::move([&](std::error_code ec) {
    e.notify();
    errc = ec;
  });

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();

  int requestsCount = 0;

  m_node.getPoolSymmetricDifferenceFunctor = [&](const std::vector<crypto::hash>& known, crypto::hash last, bool& is_actual, std::vector<cryptonote::Transaction>& new_txs, std::vector<crypto::hash>& deleted, const INode::Callback& callback) {
    is_actual = true;
    requestsCount++;

    if (requestsCount == 1) {
      callback(std::make_error_code(std::errc::invalid_argument));
    } else {
      callback(std::error_code());
    }
    return false;
  };

  m_node.notifyAboutPool();
  e.wait();
  EXPECT_NE(0, errc.value());
  m_node.notifyAboutPool(); //error, notify again
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(2, requestsCount);
}

TEST_F(BcSTest, poolSynchronizationCheckTxAdded) {
  auto tx1ptr = CryptoNote::createTransaction();
  auto tx1 = ::createTx(*tx1ptr.get());
  auto tx1hash = cryptonote::get_transaction_hash(tx1);

  std::vector<cryptonote::Transaction> newPoolAnswer = { tx1 };
  std::vector<crypto::hash> expectedKnownPoolHashes = { tx1hash };


  addConsumers(1);

  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  o1.syncFunc = std::move([&e](std::error_code) {
    e.notify();
  });

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();

  int requestsCount = 0;
  std::vector<crypto::hash> knownPool;

  m_node.getPoolSymmetricDifferenceFunctor = [&](const std::vector<crypto::hash>& known, crypto::hash last, bool& is_actual, std::vector<cryptonote::Transaction>& new_txs, std::vector<crypto::hash>& deleted, const INode::Callback& callback) {
    is_actual = true;
    requestsCount++;


    if (requestsCount == 1) {
      new_txs.assign(newPoolAnswer.begin(), newPoolAnswer.end());
    }

    if (requestsCount == 2) {
      knownPool.assign(known.begin(), known.end());
    }

    callback(std::error_code());

    return false;
  };

  m_node.notifyAboutPool();
  e.wait();
  m_node.notifyAboutPool();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(2, requestsCount);
  EXPECT_EQ(expectedKnownPoolHashes, knownPool);
}

TEST_F(BcSTest, poolSynchronizationCheckTxDeleted) {
  auto tx1ptr = CryptoNote::createTransaction();
  auto tx1 = ::createTx(*tx1ptr.get());
  auto tx1hash = cryptonote::get_transaction_hash(tx1);

  std::vector<cryptonote::Transaction> newPoolAnswer = { tx1 };
  std::vector<crypto::hash> deletedPoolAnswer = { tx1hash };
  std::vector<crypto::hash> expectedKnownPoolHashes = {};


  addConsumers(1);

  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  o1.syncFunc = std::move([&e](std::error_code) {
    e.notify();
  });

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();

  int requestsCount = 0;
  std::vector<crypto::hash> knownPool;

  m_node.getPoolSymmetricDifferenceFunctor = [&](const std::vector<crypto::hash>& known, crypto::hash last, bool& is_actual, std::vector<cryptonote::Transaction>& new_txs, std::vector<crypto::hash>& deleted, const INode::Callback& callback) {
    is_actual = true;
    requestsCount++;


    if (requestsCount == 1) {
      new_txs.assign(newPoolAnswer.begin(), newPoolAnswer.end());
    }

    if (requestsCount == 2) {
      deleted.assign(deletedPoolAnswer.begin(), deletedPoolAnswer.end());
    }

    if (requestsCount == 3) {
      knownPool.assign(known.begin(), known.end());
    }

    callback(std::error_code());

    return false;
  };

  m_node.notifyAboutPool(); // add
  e.wait();
  m_node.notifyAboutPool(); // delete
  e.wait();
  m_node.notifyAboutPool(); //getknown
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(3, requestsCount);
  EXPECT_EQ(expectedKnownPoolHashes, knownPool);
}

TEST_F(BcSTest, poolSynchronizationCheckNotififcation) {
  addConsumers(1);

  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  o1.syncFunc = std::move([&e](std::error_code) {
    e.notify();
  });

  m_sync.addObserver(&o1);
  m_sync.start();
  EXPECT_EQ(true, e.wait_for(std::chrono::milliseconds(300)));
  m_sync.stop();
}

TEST_F(BcSTest, poolSynchronizationCheckConsumersNotififcation) {
  FunctorialPoolConsumerStub c1(m_currency.genesisBlockHash());
  FunctorialPoolConsumerStub c2(m_currency.genesisBlockHash());

  c1.getKnownPoolTxIdsFunctor = [&](std::vector<crypto::hash>& ids) {};
  c2.getKnownPoolTxIdsFunctor = [&](std::vector<crypto::hash>& ids) {};

  bool c1Notified = false;
  bool c2Notified = false;
  c1.onPoolUpdatedFunctor = [&](const std::vector<cryptonote::Transaction>& new_txs, const std::vector<crypto::hash>& deleted)->std::error_code {
    c1Notified = true;
    return std::error_code();
  };

  c2.onPoolUpdatedFunctor = [&](const std::vector<cryptonote::Transaction>& new_txs, const std::vector<crypto::hash>& deleted)->std::error_code {
    c2Notified = true;
    return std::error_code();
  };

  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  o1.syncFunc = std::move([&e](std::error_code) {
    e.notify();
  });

  m_sync.addObserver(&o1);
  m_sync.addConsumer(&c1);
  m_sync.addConsumer(&c2);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  ASSERT_TRUE(c1Notified);
  ASSERT_TRUE(c2Notified);
}

TEST_F(BcSTest, poolSynchronizationCheckConsumerReturnError) {
  FunctorialPoolConsumerStub c1(m_currency.genesisBlockHash());
  FunctorialPoolConsumerStub c2(m_currency.genesisBlockHash());

  c1.getKnownPoolTxIdsFunctor = [&](std::vector<crypto::hash>& ids) {};
  c2.getKnownPoolTxIdsFunctor = [&](std::vector<crypto::hash>& ids) {};

  bool c1Notified = false;
  bool c2Notified = false;
  c1.onPoolUpdatedFunctor = [&](const std::vector<cryptonote::Transaction>& new_txs, const std::vector<crypto::hash>& deleted)->std::error_code {
    c1Notified = true;
    return std::make_error_code(std::errc::invalid_argument);
  };

  c2.onPoolUpdatedFunctor = [&](const std::vector<cryptonote::Transaction>& new_txs, const std::vector<crypto::hash>& deleted)->std::error_code {
    c2Notified = true;
    return std::make_error_code(std::errc::invalid_argument);
  };

  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = std::move([&](std::error_code ec) {
    e.notify();
    errc = ec;
  });

  m_sync.addObserver(&o1);
  m_sync.addConsumer(&c1);
  m_sync.addConsumer(&c2);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  ASSERT_TRUE(c1Notified != c2Notified);
  EXPECT_NE(0, errc.value());
}

class FunctorialBlockhainConsumerStub : public ConsumerStub {
public:

  FunctorialBlockhainConsumerStub(const crypto::hash& genesisBlockHash) : ConsumerStub(genesisBlockHash), onBlockchainDetachFunctor([](uint64_t) {}) {}

  virtual bool onNewBlocks(const CompleteBlock* blocks, uint64_t startHeight, size_t count) override {
    return onNewBlocksFunctor(blocks, startHeight, count);
  }

  virtual void onBlockchainDetach(uint64_t height) override {
    onBlockchainDetachFunctor(height);
  }

  std::function<bool(const CompleteBlock*, uint64_t, size_t)> onNewBlocksFunctor;
  std::function<void(uint64_t)> onBlockchainDetachFunctor;
};

TEST_F(BcSTest, checkINodeError) {
  addConsumers(1);
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = std::move([&](std::error_code ec) {
    e.notify();
    errc = ec;
  });

  m_node.queryBlocksFunctor = [](const std::list<crypto::hash>& knownBlockIds, uint64_t timestamp, std::list<CryptoNote::BlockCompleteEntry>& newBlocks, uint64_t& startHeight, const INode::Callback& callback) -> bool {
    callback(std::make_error_code(std::errc::invalid_argument));
    return false;
  };

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(std::make_error_code(std::errc::invalid_argument), errc);
}

TEST_F(BcSTest, checkConsumerError) {
  FunctorialBlockhainConsumerStub c(m_currency.genesisBlockHash());
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = std::move([&](std::error_code ec) {
    e.notify();
    errc = ec;
  });

  generator.generateEmptyBlocks(10);

  c.onNewBlocksFunctor = [](const CompleteBlock*, uint64_t, size_t) -> bool {
    return false;
  };

  m_sync.addObserver(&o1);
  m_sync.addConsumer(&c);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(std::make_error_code(std::errc::invalid_argument), errc);
}

TEST_F(BcSTest, checkINodeReturnBadBlock) {
  addConsumers(1);
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = std::move([&](std::error_code ec) {
    e.notify();
    errc = ec;
  });

  m_node.queryBlocksFunctor = [](const std::list<crypto::hash>& knownBlockIds, uint64_t timestamp, std::list<CryptoNote::BlockCompleteEntry>& newBlocks, uint64_t& startHeight, const INode::Callback& callback) -> bool {
    CryptoNote::BlockCompleteEntry block;
    block.block = "badblock";
    startHeight = 1;
    newBlocks.push_back(block);
    callback(std::error_code());
    return false;
  };

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(std::make_error_code(std::errc::invalid_argument), errc);
}

TEST_F(BcSTest, checkINodeReturnBadTx) {
  addConsumers(1);
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = std::move([&](std::error_code ec) {
    e.notify();
    errc = ec;
  });

  generator.generateEmptyBlocks(2);

  CryptoNote::BlockCompleteEntry bce;

  auto last_block = generator.getBlockchain().back();
  bce.blockHash = cryptonote::get_block_hash(last_block);
  bce.block = cryptonote::block_to_blob(last_block);
  bce.txs.push_back("badtx");
  

  m_node.queryBlocksFunctor = [&bce](const std::list<crypto::hash>& knownBlockIds, uint64_t timestamp, std::list<CryptoNote::BlockCompleteEntry>& newBlocks, uint64_t& startHeight, const INode::Callback& callback) -> bool {
    startHeight = 1;
    newBlocks.push_back(bce);
    callback(std::error_code());
    return false;
  };

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(std::make_error_code(std::errc::invalid_argument), errc);
}

TEST_F(BcSTest, checkBlocksRequesting) {
  FunctorialBlockhainConsumerStub c(m_currency.genesisBlockHash());
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = std::move([&](std::error_code ec) {
    e.notify();
    errc = ec;
  });


  size_t blocksExpected = 20;

  generator.generateEmptyBlocks(blocksExpected - 1); //-1 for genesis
  m_node.setGetNewBlocksLimit(3);

  size_t blocksRequested = 0;

  c.onNewBlocksFunctor = [&](const CompleteBlock*, uint64_t, size_t count) -> bool {
    blocksRequested += count;
    return true;
  };

  m_sync.addObserver(&o1);
  m_sync.addConsumer(&c);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(blocksExpected, blocksRequested);
}

TEST_F(BcSTest, checkConsumerHeightReceived) {
  FunctorialBlockhainConsumerStub c(m_currency.genesisBlockHash());
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = std::move([&](std::error_code ec) {
    e.notify();
    errc = ec;
  });


  uint64_t firstlySnchronizedHeight = 20;

  generator.generateEmptyBlocks(firstlySnchronizedHeight - 1);//-1 for genesis
  m_node.setGetNewBlocksLimit(50);

  c.onNewBlocksFunctor = [&](const CompleteBlock*, uint64_t startHeight, size_t) -> bool {
    return true;
  };

  m_sync.addObserver(&o1);
  m_sync.addConsumer(&c);
  m_sync.start();
  e.wait();
  m_sync.stop();

  generator.generateEmptyBlocks(20);

  ConsumerStub fake_c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&fake_c);
  uint64_t receivedStartHeight = 0;
  c.onNewBlocksFunctor = [&](const CompleteBlock*, uint64_t startHeight, size_t) -> bool {
    receivedStartHeight = startHeight;
    return true;
  };

  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(firstlySnchronizedHeight + 1, receivedStartHeight);
}

TEST_F(BcSTest, checkConsumerOldBlocksNotIvoked) {
  FunctorialBlockhainConsumerStub c(m_currency.genesisBlockHash());
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = std::move([&](std::error_code ec) {
    e.notify();
    errc = ec;
  });

  generator.generateEmptyBlocks(20);
  m_node.setGetNewBlocksLimit(50);

  c.onNewBlocksFunctor = [&](const CompleteBlock*, uint64_t startHeight, size_t) -> bool {
    return true;
  };

  m_sync.addObserver(&o1);
  m_sync.addConsumer(&c);
  m_sync.start();
  e.wait();
  m_sync.stop();

  ConsumerStub fake_c(m_currency.genesisBlockHash());
  m_sync.addConsumer(&fake_c);

  bool onNewBlocksInvoked = false;

  c.onNewBlocksFunctor = [&](const CompleteBlock*, uint64_t startHeight, size_t) -> bool {
    onNewBlocksInvoked = true;
    return true;
  };

  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  ASSERT_FALSE(onNewBlocksInvoked);
}

TEST_F(BcSTest, checkConsumerHeightReceivedOnDetach) {
  FunctorialBlockhainConsumerStub c(m_currency.genesisBlockHash());
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = std::move([&](std::error_code ec) {
    e.notify();
    errc = ec;
  });

  generator.generateEmptyBlocks(20);
  m_node.setGetNewBlocksLimit(50);

  c.onNewBlocksFunctor = [&](const CompleteBlock*, uint64_t startHeight, size_t) -> bool {
    return true;
  };

  m_sync.addObserver(&o1);
  m_sync.addConsumer(&c);
  m_sync.start();
  e.wait();
  m_sync.stop();

  uint64_t alternativeHeight = 10;

  m_node.startAlternativeChain(alternativeHeight);  
  generator.generateEmptyBlocks(20);

  uint64_t receivedStartHeight = 0;
  c.onNewBlocksFunctor = [&](const CompleteBlock*, uint64_t startHeight, size_t) -> bool {
    receivedStartHeight = startHeight;
    return true;
  };

  uint64_t receivedetachHeight = 0;
  c.onBlockchainDetachFunctor = [&](uint64_t detachHeight) {
    receivedetachHeight = detachHeight;
  };

  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(alternativeHeight, receivedetachHeight);
  EXPECT_EQ(alternativeHeight, receivedStartHeight);
}

TEST_F(BcSTest, checkStatePreservingBetweenSynchronizations) {
  addConsumers(1);
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = std::move([&](std::error_code ec) {
    e.notify();
    errc = ec;
  });

  generator.generateEmptyBlocks(20);

  crypto::hash lastBlockHash = cryptonote::get_block_hash(generator.getBlockchain().back());

  m_sync.addObserver(&o1);
  m_sync.start();
  e.wait();
  m_sync.stop();

  crypto::hash receivedLastBlockHash;

  m_node.queryBlocksFunctor = [&receivedLastBlockHash](const std::list<crypto::hash>& knownBlockIds, uint64_t timestamp, std::list<CryptoNote::BlockCompleteEntry>& newBlocks, uint64_t& startHeight, const INode::Callback& callback) -> bool {
    receivedLastBlockHash = knownBlockIds.front();
    startHeight = 1;
    callback(std::make_error_code(std::errc::interrupted));
    return false;
  };

  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(lastBlockHash, receivedLastBlockHash);
}

TEST_F(BcSTest, checkBlocksRerequestingOnError) {
  FunctorialBlockhainConsumerStub c(m_currency.genesisBlockHash());
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = std::move([&](std::error_code ec) {
    e.notify();
    errc = ec;
  });

  generator.generateEmptyBlocks(20);
  m_node.setGetNewBlocksLimit(10);
  
  int requestsCount = 0;
  std::list<crypto::hash> firstlyKnownBlockIdsTaken;
  std::list<crypto::hash> secondlyKnownBlockIdsTaken;

  std::vector<crypto::hash> firstlyReceivedBlocks;
  std::vector<crypto::hash> secondlyReceivedBlocks;


  c.onNewBlocksFunctor = [&](const CompleteBlock* blocks, uint64_t, size_t count) -> bool {

    if (requestsCount == 2) {
      for (size_t i = 0; i < count; ++i) {
        firstlyReceivedBlocks.push_back(blocks[i].blockHash);
      }

      return false;
    }

    if (requestsCount == 3) {
      for (size_t i = 0; i < count; ++i) {
        secondlyReceivedBlocks.push_back(blocks[i].blockHash);
      }
    }

    return true;   
  };

  m_node.queryBlocksFunctor = [&](const std::list<crypto::hash>& knownBlockIds, uint64_t timestamp, std::list<CryptoNote::BlockCompleteEntry>& newBlocks, uint64_t& startHeight, const INode::Callback& callback) -> bool {
    if (requestsCount == 1) {
      firstlyKnownBlockIdsTaken.assign(knownBlockIds.begin(), knownBlockIds.end());
    }

    if (requestsCount == 2) {
      secondlyKnownBlockIdsTaken.assign(knownBlockIds.begin(), knownBlockIds.end());
    }


    ++requestsCount;
    return true;
  };

  m_sync.addObserver(&o1);
  m_sync.addConsumer(&c);
  m_sync.start();
  e.wait();
  m_sync.stop();

  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(firstlyKnownBlockIdsTaken, secondlyKnownBlockIdsTaken);
  EXPECT_EQ(firstlyReceivedBlocks, secondlyReceivedBlocks);
}

TEST_F(BcSTest, checkTxOrder) {
  FunctorialBlockhainConsumerStub c(m_currency.genesisBlockHash());
  IBlockchainSynchronizerFunctorialObserver o1;
  EventWaiter e;
  std::error_code errc;
  o1.syncFunc = std::move([&](std::error_code ec) {
    e.notify();
    errc = ec;
  });


  auto tx1ptr = CryptoNote::createTransaction();
  auto tx2ptr = CryptoNote::createTransaction();
  auto tx3ptr = CryptoNote::createTransaction();

  auto tx1 = ::createTx(*tx1ptr.get());
  auto tx2 = ::createTx(*tx2ptr.get());
  auto tx3 = ::createTx(*tx3ptr.get());

  auto tx1hash = cryptonote::get_transaction_hash(tx1);
  auto tx2hash = cryptonote::get_transaction_hash(tx2);
  auto tx3hash = cryptonote::get_transaction_hash(tx3);


  generator.generateEmptyBlocks(2);

  CryptoNote::BlockCompleteEntry bce;

  auto last_block = generator.getBlockchain().back();
  bce.blockHash = cryptonote::get_block_hash(last_block);
  bce.block = cryptonote::block_to_blob(last_block);
  bce.txs.push_back(cryptonote::tx_to_blob(tx1));
  bce.txs.push_back(cryptonote::tx_to_blob(tx2));
  bce.txs.push_back(cryptonote::tx_to_blob(tx3));


  std::vector<crypto::hash> expectedTxHashes = { cryptonote::get_transaction_hash(last_block.minerTx), tx1hash, tx2hash, tx3hash };

  int requestNumber = 0;

  m_node.queryBlocksFunctor = [&bce, &requestNumber](const std::list<crypto::hash>& knownBlockIds, uint64_t timestamp, std::list<CryptoNote::BlockCompleteEntry>& newBlocks, uint64_t& startHeight, const INode::Callback& callback) -> bool {
    startHeight = 1;
    newBlocks.push_back(bce);
    if (requestNumber > 0) {
      callback(std::make_error_code(std::errc::interrupted));
    } else {
      callback(std::error_code());
    }

    requestNumber++;
    return false;
  };

  std::vector<crypto::hash> receivedTxHashes = {};

  c.onNewBlocksFunctor = [&](const CompleteBlock* blocks, uint64_t, size_t count) -> bool {
    for (auto& tx : blocks[count - 1].transactions) {
      auto hash = tx->getTransactionHash();
      receivedTxHashes.push_back(*reinterpret_cast<crypto::hash*>(&hash));
    }

    return true;
  };

  m_sync.addObserver(&o1);
  m_sync.addConsumer(&c);
  m_sync.start();
  e.wait();
  m_sync.stop();
  m_sync.removeObserver(&o1);
  o1.syncFunc = [](std::error_code) {};

  EXPECT_EQ(expectedTxHashes, receivedTxHashes);
}
