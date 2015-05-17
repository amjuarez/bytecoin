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

#include "InProcessNode.h"

#include <functional>
#include <boost/utility/value_init.hpp>

#include "cryptonote_core/connection_context.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/verification_context.h"
#include "cryptonote_protocol/cryptonote_protocol_handler_common.h"
#include "InProcessNodeErrors.h"

namespace CryptoNote {

InProcessNode::InProcessNode(cryptonote::ICore& core, cryptonote::ICryptonoteProtocolQuery& protocol) :
    state(NOT_INITIALIZED),
    core(core),
    protocol(protocol)
{
}

InProcessNode::~InProcessNode() {
  shutdown();
}

bool InProcessNode::addObserver(INodeObserver* observer) {
  return observerManager.add(observer);
}

bool InProcessNode::removeObserver(INodeObserver* observer) {
  return observerManager.remove(observer);
}

void InProcessNode::init(const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  std::error_code ec;

  if (state != NOT_INITIALIZED) {
    ec = make_error_code(cryptonote::error::ALREADY_INITIALIZED);
  } else {
    protocol.addObserver(this);
    core.addObserver(this);

    work.reset(new boost::asio::io_service::work(ioService));
    workerThread.reset(new std::thread(&InProcessNode::workerFunc, this));

    state = INITIALIZED;
  }

  ioService.post(std::bind(callback, ec));
}

bool InProcessNode::shutdown() {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    return false;
  }

  protocol.removeObserver(this);
  core.removeObserver(this);
  state = NOT_INITIALIZED;

  work.reset();
  ioService.stop();
  workerThread->join();
  ioService.reset();
  return true;
}

void InProcessNode::workerFunc() {
  ioService.run();
}

void InProcessNode::getNewBlocks(std::list<crypto::hash>&& knownBlockIds, std::list<cryptonote::block_complete_entry>& newBlocks,
    uint64_t& startHeight, const Callback& callback)
{
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(cryptonote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(&InProcessNode::getNewBlocksAsync,
      this,
      std::move(knownBlockIds),
      std::ref(newBlocks),
      std::ref(startHeight),
      callback
    )
  );
}

void InProcessNode::getNewBlocksAsync(std::list<crypto::hash>& knownBlockIds, std::list<cryptonote::block_complete_entry>& newBlocks,
    uint64_t& startHeight, const Callback& callback)
{
  std::error_code ec;
  {
    std::unique_lock<std::mutex> lock(mutex);
    ec = doGetNewBlocks(std::move(knownBlockIds), newBlocks, startHeight);
  }

  callback(ec);
}

//it's always protected with mutex
std::error_code InProcessNode::doGetNewBlocks(std::list<crypto::hash>&& knownBlockIds, std::list<cryptonote::block_complete_entry>& newBlocks, uint64_t& startHeight) {
  if (state != INITIALIZED) {
    return make_error_code(cryptonote::error::NOT_INITIALIZED);
  }

  try {
    uint64_t totalHeight;
    std::list<std::pair<cryptonote::Block, std::list<cryptonote::Transaction> > > bs;
    if (!core.find_blockchain_supplement(knownBlockIds, bs, totalHeight, startHeight, 1000)) {
      return make_error_code(cryptonote::error::REQUEST_ERROR);
    }

    for (auto& b : bs) {
      cryptonote::block_complete_entry be;
      be.block = cryptonote::block_to_blob(b.first);

      for (auto& t : b.second) {
        be.txs.push_back(cryptonote::tx_to_blob(t));
      }

      newBlocks.push_back(std::move(be));
    }
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(cryptonote::error::INTERNAL_NODE_ERROR);
  }

  return std::error_code();
}

void InProcessNode::getTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices,
    const Callback& callback)
{
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(cryptonote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(&InProcessNode::getTransactionOutsGlobalIndicesAsync,
      this,
      std::cref(transactionHash),
      std::ref(outsGlobalIndices),
      callback
    )
  );
}

void InProcessNode::getTransactionOutsGlobalIndicesAsync(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices,
    const Callback& callback)
{
  std::error_code ec;
  {
    std::unique_lock<std::mutex> lock(mutex);
    ec = doGetTransactionOutsGlobalIndices(transactionHash, outsGlobalIndices);
  }

  callback(ec);
}

//it's always protected with mutex
std::error_code InProcessNode::doGetTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices) {
  if (state != INITIALIZED) {
    return make_error_code(cryptonote::error::NOT_INITIALIZED);
  }

  try {
    bool r = core.get_tx_outputs_gindexs(transactionHash, outsGlobalIndices);
    if(!r) {
      return make_error_code(cryptonote::error::REQUEST_ERROR);
    }
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(cryptonote::error::INTERNAL_NODE_ERROR);
  }

  return std::error_code();
}

void InProcessNode::getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount,
    std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback)
{
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(cryptonote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(&InProcessNode::getRandomOutsByAmountsAsync,
      this,
      std::move(amounts),
      outsCount,
      std::ref(result),
      callback
    )
  );
}

void InProcessNode::getRandomOutsByAmountsAsync(std::vector<uint64_t>& amounts, uint64_t outsCount,
  std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback)
{
  std::error_code ec;
  {
    std::unique_lock<std::mutex> lock(mutex);
    ec = doGetRandomOutsByAmounts(std::move(amounts), outsCount, result);
  }

  callback(ec);
}

//it's always protected with mutex
std::error_code InProcessNode::doGetRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount, std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result) {
  if (state != INITIALIZED) {
    return make_error_code(cryptonote::error::NOT_INITIALIZED);
  }

  try {
    cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response res;
    cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request req;
    req.amounts = amounts;
    req.outs_count = outsCount;

    if(!core.get_random_outs_for_amounts(req, res)) {
      return make_error_code(cryptonote::error::REQUEST_ERROR);
    }

    result = std::move(res.outs);
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(cryptonote::error::INTERNAL_NODE_ERROR);
  }

  return std::error_code();
}


void InProcessNode::relayTransaction(const cryptonote::Transaction& transaction, const Callback& callback)
{
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(cryptonote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(&InProcessNode::relayTransactionAsync,
      this,
      transaction,
      callback
    )
  );
}

void InProcessNode::relayTransactionAsync(const cryptonote::Transaction& transaction, const Callback& callback) {
  std::error_code ec;
  {
    std::unique_lock<std::mutex> lock(mutex);
    ec = doRelayTransaction(transaction);
  }

  callback(ec);
}

//it's always protected with mutex
std::error_code InProcessNode::doRelayTransaction(const cryptonote::Transaction& transaction) {
  if (state != INITIALIZED) {
    return make_error_code(cryptonote::error::NOT_INITIALIZED);
  }

  try {
    cryptonote::blobdata txBlob = cryptonote::tx_to_blob(transaction);
    cryptonote::tx_verification_context tvc = boost::value_initialized<cryptonote::tx_verification_context>();

    if(!core.handle_incoming_tx(txBlob, tvc, false)) {
      return make_error_code(cryptonote::error::REQUEST_ERROR);
    }

    if(tvc.m_verifivation_failed) {
      return make_error_code(cryptonote::error::REQUEST_ERROR);
    }

    if(!tvc.m_should_be_relayed) {
      return make_error_code(cryptonote::error::REQUEST_ERROR);
    }

    cryptonote::cryptonote_connection_context fake_context = boost::value_initialized<cryptonote::cryptonote_connection_context>();
    cryptonote::NOTIFY_NEW_TRANSACTIONS::request r;
    r.txs.push_back(txBlob);
    core.get_protocol()->relay_transactions(r, fake_context);
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(cryptonote::error::INTERNAL_NODE_ERROR);
  }

  return std::error_code();
}

size_t InProcessNode::getPeerCount() const {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(cryptonote::error::NOT_INITIALIZED));
  }

  return protocol.getPeerCount();
}

uint64_t InProcessNode::getLastLocalBlockHeight() const {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(cryptonote::error::NOT_INITIALIZED));
  }

  uint64_t height;
  crypto::hash ignore;

  if (!core.get_blockchain_top(height, ignore)) {
    throw std::system_error(make_error_code(cryptonote::error::INTERNAL_NODE_ERROR));
  }

  return height;
}

uint64_t InProcessNode::getLastKnownBlockHeight() const {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(cryptonote::error::NOT_INITIALIZED));
  }

  return protocol.getObservedHeight();
}

uint64_t InProcessNode::getLastLocalBlockTimestamp() const {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(cryptonote::error::NOT_INITIALIZED));
  }
  lock.unlock();

  uint64_t ignore;
  crypto::hash hash;

  if (!core.get_blockchain_top(ignore, hash)) {
    throw std::system_error(make_error_code(cryptonote::error::INTERNAL_NODE_ERROR));
  }

  cryptonote::Block block;
  if (!core.getBlockByHash(hash, block)) {
    throw std::system_error(make_error_code(cryptonote::error::INTERNAL_NODE_ERROR));
  }

  return block.timestamp;
}

void InProcessNode::peerCountUpdated(size_t count) {
  observerManager.notify(&INodeObserver::peerCountUpdated, count);
}

void InProcessNode::lastKnownBlockHeightUpdated(uint64_t height) {
  observerManager.notify(&INodeObserver::lastKnownBlockHeightUpdated, height);
}

void InProcessNode::blockchainUpdated() {
  uint64_t height;
  crypto::hash ignore;

  core.get_blockchain_top(height, ignore);
  observerManager.notify(&INodeObserver::localBlockchainUpdated, height);
}

void InProcessNode::poolUpdated() {
  observerManager.notify(&INodeObserver::poolChanged);
}

void InProcessNode::queryBlocks(std::list<crypto::hash>&& knownBlockIds, uint64_t timestamp,
    std::list<BlockCompleteEntry>& newBlocks, uint64_t& startHeight, const InProcessNode::Callback& callback)
{
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(cryptonote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(&InProcessNode::queryBlocksAsync,
      this,
      std::move(knownBlockIds),
      timestamp,
      std::ref(newBlocks),
      std::ref(startHeight),
      callback
    )
  );
}

void InProcessNode::queryBlocksAsync(std::list<crypto::hash>& knownBlockIds, uint64_t timestamp,
  std::list<BlockCompleteEntry>& newBlocks, uint64_t& startHeight, const Callback& callback)
{
  std::error_code ec;
  {
    std::unique_lock<std::mutex> lock(mutex);
    ec = doQueryBlocks(std::move(knownBlockIds), timestamp, newBlocks, startHeight);
  }

  callback(ec);
}

std::error_code InProcessNode::doQueryBlocks(std::list<crypto::hash>&& knownBlockIds, uint64_t timestamp,
    std::list<BlockCompleteEntry>& newBlocks, uint64_t& startHeight) {
  uint64_t currentHeight, fullOffset;
  std::list<cryptonote::BlockFullInfo> entries;

  if (!core.queryBlocks(knownBlockIds, timestamp, startHeight, currentHeight, fullOffset, entries)) {
    return make_error_code(cryptonote::error::INTERNAL_NODE_ERROR);
  }

  for (const auto& entry: entries) {
    BlockCompleteEntry bce;
    bce.blockHash = entry.block_id;
    bce.block = entry.block;
    std::copy(entry.txs.begin(), entry.txs.end(), std::back_inserter(bce.txs));

    newBlocks.push_back(std::move(bce));
  }

  return std::error_code();
}

void InProcessNode::getPoolSymmetricDifference(std::vector<crypto::hash>&& knownPoolTxIds, crypto::hash knownBlockId, bool& isBcActual, std::vector<cryptonote::Transaction>& newTxs,
  std::vector<crypto::hash>& deletedTxIds, const Callback& callback) {

  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(cryptonote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(&InProcessNode::getPoolSymmetricDifferenceAsync,
      this,
      std::move(knownPoolTxIds),
      knownBlockId,
      std::ref(isBcActual),
      std::ref(newTxs),
      std::ref(deletedTxIds),
      callback
    )
  );
}

void InProcessNode::getPoolSymmetricDifferenceAsync(std::vector<crypto::hash>& known_pool_tx_ids, crypto::hash known_block_id, bool& is_bc_actual, std::vector<cryptonote::Transaction>& new_txs,
  std::vector<crypto::hash>& deleted_tx_ids, const Callback& callback) {
  std::error_code ec = std::error_code();

  std::unique_lock<std::mutex> lock(mutex);
  if (!core.getPoolSymmetricDifference(known_pool_tx_ids, known_block_id, is_bc_actual, new_txs, deleted_tx_ids)) {
    ec = make_error_code(cryptonote::error::INTERNAL_NODE_ERROR);
  }

  lock.unlock();
  callback(ec);
}

} //namespace CryptoNote

