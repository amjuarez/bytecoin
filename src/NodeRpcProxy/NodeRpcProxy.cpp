// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "NodeRpcProxy.h"
#include "NodeErrors.h"

#include <atomic>
#include <system_error>
#include <thread>

#include <HTTP/HttpRequest.h>
#include <HTTP/HttpResponse.h>
#include <System/ContextGroup.h>
#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/EventLock.h>
#include <System/Timer.h>
#include <CryptoNoteCore/TransactionApi.h>

#include "Common/StringTools.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"
#include "Rpc/HttpClient.h"
#include "Rpc/JsonRpc.h"

#ifndef AUTO_VAL_INIT
#define AUTO_VAL_INIT(n) boost::value_initialized<decltype(n)>()
#endif

using namespace Crypto;
using namespace Common;
using namespace System;

namespace CryptoNote {

namespace {

std::error_code interpretResponseStatus(const std::string& status) {
  if (CORE_RPC_STATUS_BUSY == status) {
    return make_error_code(error::NODE_BUSY);
  } else if (CORE_RPC_STATUS_OK != status) {
    return make_error_code(error::INTERNAL_NODE_ERROR);
  }
  return std::error_code();
}

}

NodeRpcProxy::NodeRpcProxy(const std::string& nodeHost, unsigned short nodePort) :
    m_rpcTimeout(10000),
    m_pullInterval(5000),
    m_nodeHost(nodeHost),
    m_nodePort(nodePort),
    m_lastLocalBlockTimestamp(0),
    m_connected(true) {
  resetInternalState();
}

NodeRpcProxy::~NodeRpcProxy() {
  try {
    shutdown();
  } catch (std::exception&) {
  }
}

void NodeRpcProxy::resetInternalState() {
  m_stop = false;
  m_peerCount.store(0, std::memory_order_relaxed);
  m_nodeHeight.store(0, std::memory_order_relaxed);
  m_networkHeight.store(0, std::memory_order_relaxed);
  m_lastKnowHash = CryptoNote::NULL_HASH;
  m_knownTxs.clear();
}

void NodeRpcProxy::init(const INode::Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_state != STATE_NOT_INITIALIZED) {
    callback(make_error_code(error::ALREADY_INITIALIZED));
    return;
  }

  m_state = STATE_INITIALIZING;
  resetInternalState();
  m_workerThread = std::thread([this, callback] { 
    workerThread(callback); 
  });
}

bool NodeRpcProxy::shutdown() {
  std::unique_lock<std::mutex> lock(m_mutex);

  if (m_state == STATE_NOT_INITIALIZED) {
    return true;
  } else if (m_state == STATE_INITIALIZING) {
    m_cv_initialized.wait(lock, [this] { return m_state != STATE_INITIALIZING; });
    if (m_state == STATE_NOT_INITIALIZED) {
      return true;
    }
  }

  assert(m_state == STATE_INITIALIZED);
  assert(m_dispatcher != nullptr);

  m_dispatcher->remoteSpawn([this]() {
    m_stop = true;
    // Run all spawned contexts
    m_dispatcher->yield();
  });

  if (m_workerThread.joinable()) {
    m_workerThread.join();
  }
  m_state = STATE_NOT_INITIALIZED;

  return true;
}

void NodeRpcProxy::workerThread(const INode::Callback& initialized_callback) {
  try {
    Dispatcher dispatcher;
    m_dispatcher = &dispatcher;
    ContextGroup contextGroup(dispatcher);
    m_context_group = &contextGroup;
    HttpClient httpClient(dispatcher, m_nodeHost, m_nodePort);
    m_httpClient = &httpClient;
    Event httpEvent(dispatcher);
    m_httpEvent = &httpEvent;
    m_httpEvent->set();

    {
      std::lock_guard<std::mutex> lock(m_mutex);
      assert(m_state == STATE_INITIALIZING);
      m_state = STATE_INITIALIZED;
      m_cv_initialized.notify_all();
    }

    initialized_callback(std::error_code());

    contextGroup.spawn([this]() {
      Timer pullTimer(*m_dispatcher);
      while (!m_stop) {
        updateNodeStatus();
        if (!m_stop) {
          pullTimer.sleep(std::chrono::milliseconds(m_pullInterval));
        }
      }
    });

    contextGroup.wait();
    // Make sure all remote spawns are executed
    m_dispatcher->yield();
  } catch (std::exception&) {
  }

  m_dispatcher = nullptr;
  m_context_group = nullptr;
  m_httpClient = nullptr;
  m_httpEvent = nullptr;
  m_connected = false;
  m_rpcProxyObserverManager.notify(&INodeRpcProxyObserver::connectionStatusUpdated, m_connected);
}

void NodeRpcProxy::updateNodeStatus() {
  bool updateBlockchain = true;
  while (updateBlockchain) {
    updateBlockchainStatus();
    updateBlockchain = !updatePoolStatus();
  }
}

bool NodeRpcProxy::updatePoolStatus() {
  std::vector<Crypto::Hash> knownTxs = getKnownTxsVector();
  Crypto::Hash tailBlock = m_lastKnowHash;

  bool isBcActual = false;
  std::vector<std::unique_ptr<ITransactionReader>> addedTxs;
  std::vector<Crypto::Hash> deletedTxsIds;

  std::error_code ec = doGetPoolSymmetricDifference(std::move(knownTxs), tailBlock, isBcActual, addedTxs, deletedTxsIds);
  if (ec) {
    return true;
  }

  if (!isBcActual) {
    return false;
  }

  if (!addedTxs.empty() || !deletedTxsIds.empty()) {
    updatePoolState(addedTxs, deletedTxsIds);
    m_observerManager.notify(&INodeObserver::poolChanged);
  }

  return true;
}

void NodeRpcProxy::updateBlockchainStatus() {
  CryptoNote::COMMAND_RPC_GET_LAST_BLOCK_HEADER::request req = AUTO_VAL_INIT(req);
  CryptoNote::COMMAND_RPC_GET_LAST_BLOCK_HEADER::response rsp = AUTO_VAL_INIT(rsp);

  std::error_code ec = jsonRpcCommand("getlastblockheader", req, rsp);

  if (!ec) {
    Crypto::Hash blockHash;
    if (!parse_hash256(rsp.block_header.hash, blockHash)) {
      return;
    }

    if (blockHash != m_lastKnowHash) {
      m_lastKnowHash = blockHash;
      m_nodeHeight.store(static_cast<uint32_t>(rsp.block_header.height), std::memory_order_relaxed);
      m_lastLocalBlockTimestamp.store(rsp.block_header.timestamp, std::memory_order_relaxed);
      m_observerManager.notify(&INodeObserver::localBlockchainUpdated, m_nodeHeight.load(std::memory_order_relaxed));
    }
  }

  CryptoNote::COMMAND_RPC_GET_INFO::request getInfoReq = AUTO_VAL_INIT(getInfoReq);
  CryptoNote::COMMAND_RPC_GET_INFO::response getInfoResp = AUTO_VAL_INIT(getInfoResp);

  ec = jsonCommand("/getinfo", getInfoReq, getInfoResp);
  if (!ec) {
    //a quirk to let wallets work with previous versions daemons.
    //Previous daemons didn't have the 'last_known_block_index' parameter in RPC so it may have zero value.
    auto lastKnownBlockIndex = std::max(getInfoResp.last_known_block_index, m_nodeHeight.load(std::memory_order_relaxed));
    if (m_networkHeight.load(std::memory_order_relaxed) != lastKnownBlockIndex) {
      m_networkHeight.store(lastKnownBlockIndex, std::memory_order_relaxed);
      m_observerManager.notify(&INodeObserver::lastKnownBlockHeightUpdated, m_networkHeight.load(std::memory_order_relaxed));
    }

    updatePeerCount(getInfoResp.incoming_connections_count + getInfoResp.outgoing_connections_count);
  }

  if (m_connected != m_httpClient->isConnected()) {
    m_connected = m_httpClient->isConnected();
    m_rpcProxyObserverManager.notify(&INodeRpcProxyObserver::connectionStatusUpdated, m_connected);
  }
}

void NodeRpcProxy::updatePeerCount(size_t peerCount) {
  if (peerCount != m_peerCount) {
    m_peerCount = peerCount;
    m_observerManager.notify(&INodeObserver::peerCountUpdated, m_peerCount.load(std::memory_order_relaxed));
  }
}

void NodeRpcProxy::updatePoolState(const std::vector<std::unique_ptr<ITransactionReader>>& addedTxs, const std::vector<Crypto::Hash>& deletedTxsIds) {
  for (const auto& hash : deletedTxsIds) {
    m_knownTxs.erase(hash);
  }

  for (const auto& tx : addedTxs) {
    Hash hash = tx->getTransactionHash();
    m_knownTxs.emplace(std::move(hash));
  }
}

std::vector<Crypto::Hash> NodeRpcProxy::getKnownTxsVector() const {
  return std::vector<Crypto::Hash>(m_knownTxs.begin(), m_knownTxs.end());
}

bool NodeRpcProxy::addObserver(INodeObserver* observer) {
  return m_observerManager.add(observer);
}

bool NodeRpcProxy::removeObserver(INodeObserver* observer) {
  return m_observerManager.remove(observer);
}

bool NodeRpcProxy::addObserver(CryptoNote::INodeRpcProxyObserver* observer) {
  return m_rpcProxyObserverManager.add(observer);
}

bool NodeRpcProxy::removeObserver(CryptoNote::INodeRpcProxyObserver* observer) {
  return m_rpcProxyObserverManager.remove(observer);
}

size_t NodeRpcProxy::getPeerCount() const {
  return m_peerCount.load(std::memory_order_relaxed);
}

uint32_t NodeRpcProxy::getLastLocalBlockHeight() const {
  return m_nodeHeight.load(std::memory_order_relaxed);
}

uint32_t NodeRpcProxy::getLastKnownBlockHeight() const {
  return m_networkHeight.load(std::memory_order_relaxed);
}

uint32_t NodeRpcProxy::getLocalBlockCount() const {
  return m_nodeHeight.load(std::memory_order_relaxed);
}

uint32_t NodeRpcProxy::getKnownBlockCount() const {
  return m_networkHeight.load(std::memory_order_relaxed) + 1;
}

uint64_t NodeRpcProxy::getLastLocalBlockTimestamp() const {
  return m_lastLocalBlockTimestamp;
}

void NodeRpcProxy::relayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doRelayTransaction, this, transaction), callback);
}

void NodeRpcProxy::getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount,
                                          std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& outs,
                                          const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetRandomOutsByAmounts, this, std::move(amounts), outsCount, std::ref(outs)),
    callback);
}

void NodeRpcProxy::getNewBlocks(std::vector<Crypto::Hash>&& knownBlockIds,
                                std::vector<CryptoNote::block_complete_entry>& newBlocks,
                                uint32_t& startHeight,
                                const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetNewBlocks, this, std::move(knownBlockIds), std::ref(newBlocks),
    std::ref(startHeight)), callback);
}

void NodeRpcProxy::getTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash,
                                                   std::vector<uint32_t>& outsGlobalIndices, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetTransactionOutsGlobalIndices, this, transactionHash,
    std::ref(outsGlobalIndices)), callback);
}

void NodeRpcProxy::queryBlocks(std::vector<Crypto::Hash>&& knownBlockIds, uint64_t timestamp, std::vector<BlockShortEntry>& newBlocks,
  uint32_t& startHeight, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doQueryBlocksLite, this, std::move(knownBlockIds), timestamp,
          std::ref(newBlocks), std::ref(startHeight)), callback);
}

void NodeRpcProxy::getPoolSymmetricDifference(std::vector<Crypto::Hash>&& knownPoolTxIds, Crypto::Hash knownBlockId, bool& isBcActual,
        std::vector<std::unique_ptr<ITransactionReader>>& newTxs, std::vector<Crypto::Hash>& deletedTxIds, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  scheduleRequest([this, knownPoolTxIds, knownBlockId, &isBcActual, &newTxs, &deletedTxIds] () mutable -> std::error_code {
    return this->doGetPoolSymmetricDifference(std::move(knownPoolTxIds), knownBlockId, isBcActual, newTxs, deletedTxIds); } , callback);
}

void NodeRpcProxy::getMultisignatureOutputByGlobalIndex(uint64_t amount, uint32_t gindex, MultisignatureOutput& out, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  // TODO NOT IMPLEMENTED
  callback(std::error_code());
}

void NodeRpcProxy::getBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  // TODO NOT IMPLEMENTED
  callback(std::error_code());
}

void NodeRpcProxy::getBlocks(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<BlockDetails>& blocks, uint32_t& blocksNumberWithinTimestamps, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  // TODO NOT IMPLEMENTED
  callback(std::error_code());
}

void NodeRpcProxy::getBlocks(const std::vector<Crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  // TODO NOT IMPLEMENTED
  callback(std::error_code());
}

void NodeRpcProxy::getTransactions(const std::vector<Crypto::Hash>& transactionHashes, std::vector<TransactionDetails>& transactions, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  // TODO NOT IMPLEMENTED
  callback(std::error_code());
}

void NodeRpcProxy::getPoolTransactions(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<TransactionDetails>& transactions, uint64_t& transactionsNumberWithinTimestamps, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  // TODO NOT IMPLEMENTED
  callback(std::error_code());
}

void NodeRpcProxy::getTransactionsByPaymentId(const Crypto::Hash& paymentId, std::vector<TransactionDetails>& transactions, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  // TODO NOT IMPLEMENTED
  callback(std::error_code());
}

void NodeRpcProxy::isSynchronized(bool& syncStatus, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  // TODO NOT IMPLEMENTED
  callback(std::error_code());
}

std::error_code NodeRpcProxy::doRelayTransaction(const CryptoNote::Transaction& transaction) {
  COMMAND_RPC_SEND_RAW_TX::request req;
  COMMAND_RPC_SEND_RAW_TX::response rsp;
  req.tx_as_hex = toHex(toBinaryArray(transaction));
  return jsonCommand("/sendrawtransaction", req, rsp);
}

std::error_code NodeRpcProxy::doGetRandomOutsByAmounts(std::vector<uint64_t>& amounts, uint64_t outsCount,
                                                       std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& outs) {
  COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request req = AUTO_VAL_INIT(req);
  COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response rsp = AUTO_VAL_INIT(rsp);
  req.amounts = std::move(amounts);
  req.outs_count = outsCount;

  std::error_code ec = binaryCommand("/getrandom_outs.bin", req, rsp);
  if (!ec) {
    outs = std::move(rsp.outs);
  }

  return ec;
}

std::error_code NodeRpcProxy::doGetNewBlocks(std::vector<Crypto::Hash>& knownBlockIds,
                                             std::vector<CryptoNote::block_complete_entry>& newBlocks,
                                             uint32_t& startHeight) {
  CryptoNote::COMMAND_RPC_GET_BLOCKS_FAST::request req = AUTO_VAL_INIT(req);
  CryptoNote::COMMAND_RPC_GET_BLOCKS_FAST::response rsp = AUTO_VAL_INIT(rsp);
  req.block_ids = std::move(knownBlockIds);

  std::error_code ec = binaryCommand("/getblocks.bin", req, rsp);
  if (!ec) {
    newBlocks = std::move(rsp.blocks);
    startHeight = static_cast<uint32_t>(rsp.start_height);
  }

  return ec;
}

std::error_code NodeRpcProxy::doGetTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash,
                                                                std::vector<uint32_t>& outsGlobalIndices) {
  CryptoNote::COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::request req = AUTO_VAL_INIT(req);
  CryptoNote::COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::response rsp = AUTO_VAL_INIT(rsp);
  req.txid = transactionHash;

  std::error_code ec = binaryCommand("/get_o_indexes.bin", req, rsp);
  if (!ec) {
    outsGlobalIndices.clear();
    for (auto idx : rsp.o_indexes) {
      outsGlobalIndices.push_back(static_cast<uint32_t>(idx));
    }
  }

  return ec;
}

std::error_code NodeRpcProxy::doQueryBlocksLite(const std::vector<Crypto::Hash>& knownBlockIds, uint64_t timestamp,
        std::vector<CryptoNote::BlockShortEntry>& newBlocks, uint32_t& startHeight) {
  CryptoNote::COMMAND_RPC_QUERY_BLOCKS_LITE::request req = AUTO_VAL_INIT(req);
  CryptoNote::COMMAND_RPC_QUERY_BLOCKS_LITE::response rsp = AUTO_VAL_INIT(rsp);

  req.blockIds = knownBlockIds;
  req.timestamp = timestamp;

  std::error_code ec = binaryCommand("/queryblockslite.bin", req, rsp);
  if (ec) {
    return ec;
  }

  startHeight = static_cast<uint32_t>(rsp.startHeight);

  for (auto& item: rsp.items) {
    BlockShortEntry bse;
    bse.hasBlock = false;

    bse.blockHash = std::move(item.blockId);
    if (!item.block.empty()) {
      if (!fromBinaryArray(bse.block, asBinaryArray(item.block))) {
        return std::make_error_code(std::errc::invalid_argument);
      }

      bse.hasBlock = true;
    }

    for (const auto& txp: item.txPrefixes) {
      TransactionShortInfo tsi;
      tsi.txId = txp.txHash;
      tsi.txPrefix = txp.txPrefix;
      bse.txsShortInfo.push_back(std::move(tsi));
    }

    newBlocks.push_back(std::move(bse));
  }

  return std::error_code();
}

std::error_code NodeRpcProxy::doGetPoolSymmetricDifference(std::vector<Crypto::Hash>&& knownPoolTxIds, Crypto::Hash knownBlockId, bool& isBcActual,
        std::vector<std::unique_ptr<ITransactionReader>>& newTxs, std::vector<Crypto::Hash>& deletedTxIds) {
  CryptoNote::COMMAND_RPC_GET_POOL_CHANGES_LITE::request req = AUTO_VAL_INIT(req);
  CryptoNote::COMMAND_RPC_GET_POOL_CHANGES_LITE::response rsp = AUTO_VAL_INIT(rsp);

  req.tailBlockId = knownBlockId;
  req.knownTxsIds = knownPoolTxIds;

  std::error_code ec = binaryCommand("/get_pool_changes_lite.bin", req, rsp);

  if (ec) {
    return ec;
  }

  isBcActual = rsp.isTailBlockActual;

  deletedTxIds = std::move(rsp.deletedTxsIds);

  for (const auto& tpi : rsp.addedTxs) {
    newTxs.push_back(createTransactionPrefix(tpi.txPrefix, tpi.txHash));
  }

  return ec;
}

void NodeRpcProxy::scheduleRequest(std::function<std::error_code()>&& procedure, const Callback& callback) {
  // callback is located on stack, so copy it inside binder
  class Wrapper {
  public:
    Wrapper(std::function<void(std::function<std::error_code()>&, Callback&)>&& _func,
            std::function<std::error_code()>&& _procedure, const Callback& _callback)
        : func(std::move(_func)), procedure(std::move(_procedure)), callback(std::move(_callback)) {
    }
    Wrapper(const Wrapper& other)
        : func(other.func), procedure(other.procedure), callback(other.callback) {
    }
    Wrapper(Wrapper&& other) // must be noexcept
        : func(std::move(other.func)), procedure(std::move(other.procedure)), callback(std::move(other.callback)) {
    }
    void operator()() {
      func(procedure, callback);
    }
  private:
    std::function<void(std::function<std::error_code()>&, Callback&)> func;
    std::function<std::error_code()> procedure;
    Callback callback;
  };
  assert(m_dispatcher != nullptr && m_context_group != nullptr);
  m_dispatcher->remoteSpawn(Wrapper([this](std::function<std::error_code()>& procedure, Callback& callback) {
    m_context_group->spawn(Wrapper([this](std::function<std::error_code()>& procedure, const Callback& callback) {
        if (m_stop) {
          callback(std::make_error_code(std::errc::operation_canceled));
        } else {
          std::error_code ec = procedure();
          if (m_connected != m_httpClient->isConnected()) {
            m_connected = m_httpClient->isConnected();
            m_rpcProxyObserverManager.notify(&INodeRpcProxyObserver::connectionStatusUpdated, m_connected);
          }
          callback(m_stop ? std::make_error_code(std::errc::operation_canceled) : ec);
        }
      }, std::move(procedure), std::move(callback)));
    }, std::move(procedure), callback));
}

template <typename Request, typename Response>
std::error_code NodeRpcProxy::binaryCommand(const std::string& url, const Request& req, Response& res) {
  std::error_code ec;

  try {
    EventLock eventLock(*m_httpEvent);
    invokeBinaryCommand(*m_httpClient, url, req, res);
    ec = interpretResponseStatus(res.status);
  } catch (const ConnectException&) {
    ec = make_error_code(error::CONNECT_ERROR);
  } catch (const std::exception&) {
    ec = make_error_code(error::NETWORK_ERROR);
  }

  return ec;
}

template <typename Request, typename Response>
std::error_code NodeRpcProxy::jsonCommand(const std::string& url, const Request& req, Response& res) {
  std::error_code ec;

  try {
    EventLock eventLock(*m_httpEvent);
    invokeJsonCommand(*m_httpClient, url, req, res);
    ec = interpretResponseStatus(res.status);
  } catch (const ConnectException&) {
    ec = make_error_code(error::CONNECT_ERROR);
  } catch (const std::exception&) {
    ec = make_error_code(error::NETWORK_ERROR);
  }

  return ec;
}

template <typename Request, typename Response>
std::error_code NodeRpcProxy::jsonRpcCommand(const std::string& method, const Request& req, Response& res) {
  std::error_code ec = make_error_code(error::INTERNAL_NODE_ERROR);

  try {
    EventLock eventLock(*m_httpEvent);

    JsonRpc::JsonRpcRequest jsReq;

    jsReq.setMethod(method);
    jsReq.setParams(req);

    HttpRequest httpReq;
    HttpResponse httpRes;

    httpReq.setUrl("/json_rpc");
    httpReq.setBody(jsReq.getBody());

    m_httpClient->request(httpReq, httpRes);

    JsonRpc::JsonRpcResponse jsRes;

    if (httpRes.getStatus() == HttpResponse::STATUS_200) {
      jsRes.parse(httpRes.getBody());
      if (jsRes.getResult(res)) {
        ec = interpretResponseStatus(res.status);
      }
    }
  } catch (const ConnectException&) {
    ec = make_error_code(error::CONNECT_ERROR);
  } catch (const std::exception&) {
    ec = make_error_code(error::NETWORK_ERROR);
  }

  return ec;
}

}
