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

#include <string>
#include <thread>

#include <boost/asio/io_service.hpp>
#include <boost/asio/deadline_timer.hpp>

#include "Common/ObserverManager.h"
#include "InitState.h"
#include "INode.h"

namespace CryptoNote {

class HttpClient;

class NodeRpcProxy : public CryptoNote::INode {
public:
  NodeRpcProxy(const std::string& nodeHost, unsigned short nodePort);
  virtual ~NodeRpcProxy();

  virtual bool addObserver(CryptoNote::INodeObserver* observer);
  virtual bool removeObserver(CryptoNote::INodeObserver* observer);

  virtual void init(const Callback& callback);
  virtual bool shutdown();

  virtual size_t getPeerCount() const;
  virtual uint64_t getLastLocalBlockHeight() const;
  virtual uint64_t getLastKnownBlockHeight() const;
  virtual uint64_t getLocalBlockCount() const override;
  virtual uint64_t getKnownBlockCount() const override;
  virtual uint64_t getLastLocalBlockTimestamp() const override;

  virtual void relayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback);
  virtual void getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount, std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback);
  virtual void getNewBlocks(std::list<crypto::hash>&& knownBlockIds, std::list<CryptoNote::block_complete_entry>& newBlocks, uint64_t& startHeight, const Callback& callback);
  virtual void getTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices, const Callback& callback);
  virtual void queryBlocks(std::list<crypto::hash>&& knownBlockIds, uint64_t timestamp, std::list<CryptoNote::BlockCompleteEntry>& newBlocks, uint64_t& startHeight, const Callback& callback) override;
  // TODO INodeObserver::poolChanged() notification NOT implemented!!!
  virtual void getPoolSymmetricDifference(std::vector<crypto::hash>&& knownTxsIds, crypto::hash tailBlockId,
                                          bool& isTailBlockActual, std::vector<CryptoNote::Transaction>& addedTxs,
                                          std::vector<crypto::hash>& deletedTxsIds, const Callback& callback) override;
  virtual void getBlocks(const std::vector<uint64_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks, const Callback& callback) override;
  virtual void getBlocks(const std::vector<crypto::hash>& blockHashes, std::vector<BlockDetails>& blocks, const Callback& callback) override;
  virtual void getTransactions(const std::vector<crypto::hash>& transactionHashes, std::vector<TransactionDetails>& transactions, const Callback& callback) override;
  virtual void isSynchronized(bool& syncStatus, const Callback& callback) override;

  unsigned int rpcTimeout() const { return m_rpcTimeout; }
  void rpcTimeout(unsigned int val) { m_rpcTimeout = val; }

private:
  void resetInternalState();
  void workerThread(const Callback& initialized_callback);

  void pullNodeStatusAndScheduleTheNext();
  void updateNodeStatus();
  void updatePeerCount();

  void doRelayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback);
  void doGetRandomOutsByAmounts(std::vector<uint64_t>& amounts, uint64_t outsCount, std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback);
  void doGetNewBlocks(std::list<crypto::hash>& knownBlockIds, std::list<CryptoNote::block_complete_entry>& newBlocks, uint64_t& startHeight, const Callback& callback);
  void doGetTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices, const Callback& callback);
  void doQueryBlocks(const std::list<crypto::hash>& knownBlockIds, uint64_t timestamp, std::list<CryptoNote::BlockCompleteEntry>& newBlocks, uint64_t& startHeight, const Callback& callback);
  void doGetPoolSymmetricDifference(const std::vector<crypto::hash>& knownTxsIds, const crypto::hash& tailBlockId,
                                    bool& isTailBlockActual, std::vector<CryptoNote::Transaction>& addedTxs,
                                    std::vector<crypto::hash>& deletedTxsIds, const Callback& callback);

private:
  tools::InitState m_initState;
  std::thread m_workerThread;
  boost::asio::io_service m_ioService;
  tools::ObserverManager<CryptoNote::INodeObserver> m_observerManager;

  const std::string m_nodeHost;
  const unsigned short m_nodePort;
  unsigned int m_rpcTimeout;
  HttpClient* m_httpClient = nullptr;

  boost::asio::deadline_timer m_pullTimer;
  uint64_t m_pullInterval;

  // Internal state
  std::atomic<size_t> m_peerCount;
  std::atomic<uint64_t> m_nodeHeight;
  std::atomic<uint64_t> m_networkHeight;
  crypto::hash m_lastKnowHash;
  uint64_t m_lastLocalBlockTimestamp;
};

}
