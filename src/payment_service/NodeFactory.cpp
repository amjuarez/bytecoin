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

#include "NodeFactory.h"

#include "node_rpc_proxy/NodeRpcProxy.h"
#include <memory>
#include <future>

namespace PaymentService {

class NodeRpcStub: public CryptoNote::INode {
public:
  virtual ~NodeRpcStub() {}
  virtual bool addObserver(CryptoNote::INodeObserver* observer) { return true; }
  virtual bool removeObserver(CryptoNote::INodeObserver* observer) { return true; }

  virtual void init(const Callback& callback) { }
  virtual bool shutdown() { return true; }

  virtual size_t getPeerCount() const { return 0; }
  virtual uint64_t getLastLocalBlockHeight() const { return 0; }
  virtual uint64_t getLastKnownBlockHeight() const { return 0; }
  virtual uint64_t getLocalBlockCount() const override { return 0; }
  virtual uint64_t getKnownBlockCount() const override { return 0; }
  virtual uint64_t getLastLocalBlockTimestamp() const { return 0; }

  virtual void relayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback) { callback(std::error_code()); }
  virtual void getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount,
    std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback) { }
  virtual void getNewBlocks(std::list<crypto::hash>&& knownBlockIds, std::list<CryptoNote::block_complete_entry>& newBlocks, uint64_t& startHeight, const Callback& callback) { 
    startHeight = 0;
    callback(std::error_code());
  }
  virtual void getTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices, const Callback& callback) { }

  virtual void queryBlocks(std::list<crypto::hash>&& knownBlockIds, uint64_t timestamp, std::list<CryptoNote::BlockCompleteEntry>& newBlocks,
      uint64_t& startHeight, const CryptoNote::INode::Callback& callback) {
    startHeight = 0;
    callback(std::error_code());
  }

  virtual void getPoolSymmetricDifference(std::vector<crypto::hash>&& known_pool_tx_ids, crypto::hash known_block_id,
    bool& is_bc_actual, std::vector<CryptoNote::Transaction>& new_txs, std::vector<crypto::hash>& deleted_tx_ids,
    const Callback& callback) { 
    is_bc_actual = true;
    callback(std::error_code());
  }

  virtual void getBlocks(const std::vector<uint64_t>& blockHeights, std::vector<std::vector<CryptoNote::BlockDetails>>& blocks, 
    const Callback& callback) override { }

  virtual void getBlocks(const std::vector<crypto::hash>& blockHashes, std::vector<CryptoNote::BlockDetails>& blocks,
    const Callback& callback) override { }

  virtual void getTransactions(const std::vector<crypto::hash>& transactionHashes, std::vector<CryptoNote::TransactionDetails>& transactions,
    const Callback& callback) override { }

  virtual void isSynchronized(bool& syncStatus, const Callback& callback) override { }

};


class NodeInitObserver {
public:
  NodeInitObserver() {}

  void initCompleted(std::error_code result) {
    initPromise.set_value(result);
  }

  void waitForInitEnd() {
    auto future = initPromise.get_future();

    std::error_code ec = future.get();
    if (ec) {
      throw std::system_error(ec);
    }
    return;
  }

private:
  std::promise<std::error_code> initPromise;
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
