// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "NodeRpcProxy.h"

#include <atomic>
#include <system_error>
#include <thread>

#include "cryptonote_core/cryptonote_format_utils.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "storages/http_abstract_invoke.h"
#include "NodeErrors.h"

namespace cryptonote {

using namespace CryptoNote;

namespace {
  std::error_code interpretJsonRpcResponse(bool ok, const std::string& status) {
    if (!ok) {
      return make_error_code(error::NETWORK_ERROR);
    } else if (CORE_RPC_STATUS_BUSY == status) {
      return make_error_code(error::NODE_BUSY);
    } else if (CORE_RPC_STATUS_OK != status) {
      return make_error_code(error::INTERNAL_NODE_ERROR);
    }
    return std::error_code();
  }
}

NodeRpcProxy::NodeRpcProxy(const std::string& nodeHost, unsigned short nodePort)
  : m_nodeAddress("http://" + nodeHost + ":" + std::to_string(nodePort))
  , m_rpcTimeout(10000)
  , m_pullTimer(m_ioService)
  , m_pullInterval(10000) {
  resetInternalState();
}

NodeRpcProxy::~NodeRpcProxy() {
  shutdown();
}

void NodeRpcProxy::resetInternalState() {
  m_ioService.reset();
  m_observerManager.clear();

  m_peerCount = 0;
  m_nodeHeight = 0;
  m_networkHeight = 0;
  m_lastKnowHash = cryptonote::null_hash;
}

void NodeRpcProxy::init(const INode::Callback& callback) {
  if (!m_initState.beginInit()) {
    callback(make_error_code(error::ALREADY_INITIALIZED));
    return;
  }

  resetInternalState();
  m_workerThread = std::thread(std::bind(&NodeRpcProxy::workerThread, this, callback));
}

bool NodeRpcProxy::shutdown() {
  if (!m_initState.beginShutdown()) {
    return false;
  }

  boost::system::error_code ignored_ec;
  m_pullTimer.cancel(ignored_ec);
  m_ioService.stop();
  m_workerThread.join();

  m_initState.endShutdown();
  return true;
}

void NodeRpcProxy::workerThread(const INode::Callback& initialized_callback) {
  if (!m_initState.endInit()) {
    return;
  }

  initialized_callback(std::error_code());

  pullNodeStatusAndScheduleTheNext();

  while (!m_ioService.stopped()) {
    m_ioService.run_one();
  }
}

void NodeRpcProxy::pullNodeStatusAndScheduleTheNext() {
  updateNodeStatus();

  m_pullTimer.expires_from_now(boost::posix_time::milliseconds(m_pullInterval));
  m_pullTimer.async_wait([=](const boost::system::error_code& ec) {
    if (ec != boost::asio::error::operation_aborted) {
      pullNodeStatusAndScheduleTheNext();
    }
  });
}

void NodeRpcProxy::updateNodeStatus() {
  cryptonote::COMMAND_RPC_GET_LAST_BLOCK_HEADER::request req = AUTO_VAL_INIT(req);
  cryptonote::COMMAND_RPC_GET_LAST_BLOCK_HEADER::response rsp = AUTO_VAL_INIT(rsp);
  bool r = epee::net_utils::invoke_http_json_rpc(m_nodeAddress + "/json_rpc", "getlastblockheader", req, rsp, m_httpClient, m_rpcTimeout);
  std::error_code ec = interpretJsonRpcResponse(r, rsp.status);
  if (!ec) {
    crypto::hash blockHash;
    if (!parse_hash256(rsp.block_header.hash, blockHash)) {
      LOG_ERROR("Invalid block hash format: " << rsp.block_header.hash);
      return;
    }

    if (blockHash != m_lastKnowHash) {
      m_lastKnowHash = blockHash;
      m_nodeHeight = rsp.block_header.height;
      // TODO request and update network height
      m_networkHeight = m_nodeHeight;
      m_observerManager.notify(&INodeObserver::lastKnownBlockHeightUpdated, m_networkHeight);
      //if (m_networkHeight != rsp.block_header.network_height) {
      //  m_networkHeight = rsp.block_header.network_height;
      //  m_observerManager.notify(&INodeObserver::lastKnownBlockHeightUpdated, m_networkHeight);
      //}
      m_observerManager.notify(&INodeObserver::localBlockchainUpdated, m_nodeHeight);
    }
  } else {
    LOG_PRINT_L2("Failed to invoke getlastblockheader: " << ec.message() << ':' << ec.value());
  }

  updatePeerCount();
}

void NodeRpcProxy::updatePeerCount() {
  cryptonote::COMMAND_RPC_GET_INFO::request req = AUTO_VAL_INIT(req);
  cryptonote::COMMAND_RPC_GET_INFO::response rsp = AUTO_VAL_INIT(rsp);
  bool r = epee::net_utils::invoke_http_json_remote_command2(m_nodeAddress + "/getinfo", req, rsp, m_httpClient, m_rpcTimeout);
  std::error_code ec = interpretJsonRpcResponse(r, rsp.status);
  if (!ec) {
    size_t peerCount = rsp.incoming_connections_count + rsp.outgoing_connections_count;
    if (peerCount != m_peerCount) {
      m_peerCount = peerCount;
      m_observerManager.notify(&INodeObserver::peerCountUpdated, m_peerCount);
    }
  } else {
    LOG_PRINT_L2("Failed to invoke getinfo: " << ec.message() << ':' << ec.value());
  }
}

bool NodeRpcProxy::addObserver(INodeObserver* observer) {
  return m_observerManager.add(observer);
}

bool NodeRpcProxy::removeObserver(INodeObserver* observer) {
  return m_observerManager.remove(observer);
}

size_t NodeRpcProxy::getPeerCount() const {
  return m_peerCount;
}

uint64_t NodeRpcProxy::getLastLocalBlockHeight() const {
  return m_nodeHeight;
}

uint64_t NodeRpcProxy::getLastKnownBlockHeight() const {
  return m_networkHeight;
}

void NodeRpcProxy::relayTransaction(const cryptonote::transaction& transaction, const Callback& callback) {
  if (!m_initState.initialized()) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  // TODO: m_ioService.stop() won't inkove callback(aborted). Fix it
  m_ioService.post(std::bind(&NodeRpcProxy::doRelayTransaction, this, transaction, callback));
}

void NodeRpcProxy::getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount, std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& outs, const Callback& callback) {
  if (!m_initState.initialized()) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  m_ioService.post(std::bind(&NodeRpcProxy::doGetRandomOutsByAmounts, this, std::move(amounts), outsCount, std::ref(outs), callback));
}

void NodeRpcProxy::getNewBlocks(std::list<crypto::hash>&& knownBlockIds, std::list<cryptonote::block_complete_entry>& newBlocks, uint64_t& startHeight, const Callback& callback) {
  if (!m_initState.initialized()) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  m_ioService.post(std::bind(&NodeRpcProxy::doGetNewBlocks, this, std::move(knownBlockIds), std::ref(newBlocks), std::ref(startHeight), callback));
}

void NodeRpcProxy::getTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices, const Callback& callback) {
  if (!m_initState.initialized()) {
    callback(make_error_code(error::NOT_INITIALIZED));
    return;
  }

  m_ioService.post(std::bind(&NodeRpcProxy::doGetTransactionOutsGlobalIndices, this, transactionHash, std::ref(outsGlobalIndices), callback));
}

void NodeRpcProxy::doRelayTransaction(const cryptonote::transaction& transaction, const Callback& callback) {
  COMMAND_RPC_SEND_RAW_TX::request req;
  COMMAND_RPC_SEND_RAW_TX::response rsp;
  req.tx_as_hex = epee::string_tools::buff_to_hex_nodelimer(cryptonote::tx_to_blob(transaction));
  bool r = epee::net_utils::invoke_http_json_remote_command2(m_nodeAddress + "/sendrawtransaction", req, rsp, m_httpClient, m_rpcTimeout);
  std::error_code ec = interpretJsonRpcResponse(r, rsp.status);
  callback(ec);
}

void NodeRpcProxy::doGetRandomOutsByAmounts(std::vector<uint64_t>& amounts, uint64_t outsCount, std::vector<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& outs, const Callback& callback) {
  COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request req = AUTO_VAL_INIT(req);
  COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response rsp = AUTO_VAL_INIT(rsp);
  req.amounts = std::move(amounts);
  req.outs_count = outsCount;
  bool r = epee::net_utils::invoke_http_bin_remote_command2(m_nodeAddress + "/getrandom_outs.bin", req, rsp, m_httpClient, m_rpcTimeout);
  std::error_code ec = interpretJsonRpcResponse(r, rsp.status);
  if (!ec) {
    outs = std::move(rsp.outs);
  }
  callback(ec);
}

void NodeRpcProxy::doGetNewBlocks(std::list<crypto::hash>& knownBlockIds, std::list<cryptonote::block_complete_entry>& newBlocks, uint64_t& startHeight, const Callback& callback) {
  cryptonote::COMMAND_RPC_GET_BLOCKS_FAST::request req = AUTO_VAL_INIT(req);
  cryptonote::COMMAND_RPC_GET_BLOCKS_FAST::response rsp = AUTO_VAL_INIT(rsp);
  req.block_ids = std::move(knownBlockIds);
  bool r = epee::net_utils::invoke_http_bin_remote_command2(m_nodeAddress + "/getblocks.bin", req, rsp, m_httpClient, m_rpcTimeout);
  std::error_code ec = interpretJsonRpcResponse(r, rsp.status);
  if (!ec) {
    newBlocks = std::move(rsp.blocks);
    startHeight = rsp.start_height;
  }
  callback(ec);
}

void NodeRpcProxy::doGetTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices, const Callback& callback) {
  cryptonote::COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::request req = AUTO_VAL_INIT(req);
  cryptonote::COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::response rsp = AUTO_VAL_INIT(rsp);
  req.txid = transactionHash;
  bool r = epee::net_utils::invoke_http_bin_remote_command2(m_nodeAddress + "/get_o_indexes.bin", req, rsp, m_httpClient, m_rpcTimeout);
  std::error_code ec = interpretJsonRpcResponse(r, rsp.status);
  if (!ec) {
    outsGlobalIndices = std::move(rsp.o_indexes);
  }
  callback(ec);
}

}
