// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <thread>

#include <boost/asio/io_service.hpp>

#include "common/ObserverManager.h"
#include "include_base_utils.h"
#include "net/http_client.h"
#include "InitState.h"
#include "INode.h"

namespace cryptonote {

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

  virtual void relayTransaction(const cryptonote::transaction& transaction, const Callback& callback);
  virtual void getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount, std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback);
  virtual void getNewBlocks(std::list<crypto::hash>&& knownBlockIds, std::list<cryptonote::block_complete_entry>& newBlocks, uint64_t& startHeight, const Callback& callback);
  virtual void getTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices, const Callback& callback);

  unsigned int rpcTimeout() const { return m_rpcTimeout; }
  void rpcTimeout(unsigned int val) { m_rpcTimeout = val; }

private:
  void resetInternalState();
  void workerThread(const Callback& initialized_callback);

  void pullNodeStatusAndScheduleTheNext();
  void updateNodeStatus();
  void updatePeerCount();

  void doRelayTransaction(const cryptonote::transaction& transaction, const Callback& callback);
  void doGetRandomOutsByAmounts(std::vector<uint64_t>& amounts, uint64_t outsCount, std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback);
  void doGetNewBlocks(std::list<crypto::hash>& knownBlockIds, std::list<cryptonote::block_complete_entry>& newBlocks, uint64_t& startHeight, const Callback& callback);
  void doGetTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices, const Callback& callback);

private:
  tools::InitState m_initState;
  std::thread m_workerThread;
  boost::asio::io_service m_ioService;
  tools::ObserverManager<CryptoNote::INodeObserver> m_observerManager;

  std::string m_nodeAddress;
  unsigned int m_rpcTimeout;
  epee::net_utils::http::http_simple_client m_httpClient;

  boost::asio::deadline_timer m_pullTimer;
  uint64_t m_pullInterval;

  // Internal state
  size_t m_peerCount;
  uint64_t m_nodeHeight;
  uint64_t m_networkHeight;
  crypto::hash m_lastKnowHash;
};

}
