// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "NodeFactory.h"

#include "NodeRpcProxy/NodeRpcProxy.h"
#include <memory>
#include <future>

namespace PaymentService {

class NodeRpcStub: public CryptoNote::INode {
public:
  virtual ~NodeRpcStub() {}
  virtual bool addObserver(CryptoNote::INodeObserver* observer) override { return true; }
  virtual bool removeObserver(CryptoNote::INodeObserver* observer) override { return true; }

  virtual void init(const Callback& callback) override { }
  virtual bool shutdown() override { return true; }

  virtual size_t getPeerCount() const override { return 0; }
  virtual uint32_t getLastLocalBlockHeight() const override { return 0; }
  virtual uint32_t getLastKnownBlockHeight() const override { return 0; }
  virtual uint32_t getLocalBlockCount() const override { return 0; }
  virtual uint32_t getKnownBlockCount() const override { return 0; }
  virtual uint64_t getLastLocalBlockTimestamp() const override { return 0; }

  virtual void relayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback) override { callback(std::error_code()); }
  virtual void getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount,
    std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback) override {
  }
  virtual void getNewBlocks(std::vector<Crypto::Hash>&& knownBlockIds, std::vector<CryptoNote::block_complete_entry>& newBlocks, uint32_t& startHeight, const Callback& callback) override {
    startHeight = 0;
    callback(std::error_code());
  }
  virtual void getTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash, std::vector<uint32_t>& outsGlobalIndices, const Callback& callback) override { }

  virtual void queryBlocks(std::vector<Crypto::Hash>&& knownBlockIds, uint64_t timestamp, std::vector<CryptoNote::BlockShortEntry>& newBlocks,
    uint32_t& startHeight, const Callback& callback) override {
    startHeight = 0;
    callback(std::error_code());
  };

  virtual void getPoolSymmetricDifference(std::vector<Crypto::Hash>&& knownPoolTxIds, Crypto::Hash knownBlockId, bool& isBcActual,
          std::vector<std::unique_ptr<CryptoNote::ITransactionReader>>& newTxs, std::vector<Crypto::Hash>& deletedTxIds, const Callback& callback) override {
    isBcActual = true;
    callback(std::error_code());
  }

  virtual void getBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<CryptoNote::BlockDetails>>& blocks,
    const Callback& callback) override { }

  virtual void getBlocks(const std::vector<Crypto::Hash>& blockHashes, std::vector<CryptoNote::BlockDetails>& blocks,
    const Callback& callback) override { }

  virtual void getBlocks(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<CryptoNote::BlockDetails>& blocks, uint32_t& blocksNumberWithinTimestamps,
    const Callback& callback) override { }

  virtual void getTransactions(const std::vector<Crypto::Hash>& transactionHashes, std::vector<CryptoNote::TransactionDetails>& transactions,
    const Callback& callback) override { }

  virtual void getPoolTransactions(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<CryptoNote::TransactionDetails>& transactions, uint64_t& transactionsNumberWithinTimestamps,
    const Callback& callback) override { }

  virtual void getTransactionsByPaymentId(const Crypto::Hash& paymentId, std::vector<CryptoNote::TransactionDetails>& transactions, 
    const Callback& callback) override { }

  virtual void getMultisignatureOutputByGlobalIndex(uint64_t amount, uint32_t gindex, CryptoNote::MultisignatureOutput& out,
    const Callback& callback) override { }

  virtual void isSynchronized(bool& syncStatus, const Callback& callback) override { }

};


class NodeInitObserver {
public:
  NodeInitObserver() {
    initFuture = initPromise.get_future();
  }

  void initCompleted(std::error_code result) {
    initPromise.set_value(result);
  }

  void waitForInitEnd() {
    std::error_code ec = initFuture.get();
    if (ec) {
      throw std::system_error(ec);
    }
    return;
  }

private:
  std::promise<std::error_code> initPromise;
  std::future<std::error_code> initFuture;
};

NodeFactory::NodeFactory() {
}

NodeFactory::~NodeFactory() {
}

CryptoNote::INode* NodeFactory::createNode(const std::string& daemonAddress, uint16_t daemonPort) {
  std::unique_ptr<CryptoNote::INode> node(new CryptoNote::NodeRpcProxy(daemonAddress, daemonPort));

  NodeInitObserver initObserver;
  node->init(std::bind(&NodeInitObserver::initCompleted, &initObserver, std::placeholders::_1));
  initObserver.waitForInitEnd();

  return node.release();
}

CryptoNote::INode* NodeFactory::createNodeStub() {
  return new NodeRpcStub();
}

}
