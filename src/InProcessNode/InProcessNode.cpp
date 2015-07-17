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

#include "InProcessNode.h"

#include <functional>
#include <boost/utility/value_init.hpp>

#include "Common/StringTools.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/verification_context.h"
#include "cryptonote_protocol/cryptonote_protocol_handler_common.h"
#include "InProcessNodeErrors.h"

namespace CryptoNote {

InProcessNode::InProcessNode(CryptoNote::ICore& core, CryptoNote::ICryptonoteProtocolQuery& protocol) :
    state(NOT_INITIALIZED),
    core(core),
    protocol(protocol),
    blockchainExplorerDataBuilder(core, protocol)
{
}

InProcessNode::~InProcessNode() {
  doShutdown();
}

bool InProcessNode::addObserver(INodeObserver* observer) {
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }

  return observerManager.add(observer);
}

bool InProcessNode::removeObserver(INodeObserver* observer) {
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }

  return observerManager.remove(observer);
}

void InProcessNode::init(const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  std::error_code ec;

  if (state != NOT_INITIALIZED) {
    ec = make_error_code(CryptoNote::error::ALREADY_INITIALIZED);
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
  return doShutdown();
}

bool InProcessNode::doShutdown() {
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

void InProcessNode::getNewBlocks(std::list<crypto::hash>&& knownBlockIds, std::list<CryptoNote::block_complete_entry>& newBlocks,
    uint64_t& startHeight, const Callback& callback)
{
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
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

void InProcessNode::getNewBlocksAsync(std::list<crypto::hash>& knownBlockIds, std::list<CryptoNote::block_complete_entry>& newBlocks,
    uint64_t& startHeight, const Callback& callback)
{
  std::error_code ec = doGetNewBlocks(std::move(knownBlockIds), newBlocks, startHeight);
  callback(ec);
}

//it's always protected with mutex
std::error_code InProcessNode::doGetNewBlocks(std::list<crypto::hash>&& knownBlockIds, std::list<CryptoNote::block_complete_entry>& newBlocks, uint64_t& startHeight) {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      return make_error_code(CryptoNote::error::NOT_INITIALIZED);
    }
  }

  try {
    uint64_t totalHeight;
    std::list<std::pair<CryptoNote::Block, std::list<CryptoNote::Transaction> > > bs;
    if (!core.find_blockchain_supplement(knownBlockIds, bs, totalHeight, startHeight, 1000)) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }

    for (auto& b : bs) {
      CryptoNote::block_complete_entry be;
      be.block = CryptoNote::block_to_blob(b.first);

      for (auto& t : b.second) {
        be.txs.push_back(CryptoNote::tx_to_blob(t));
      }

      newBlocks.push_back(std::move(be));
    }
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }

  return std::error_code();
}

void InProcessNode::getTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices,
    const Callback& callback)
{
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
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
  std::error_code ec = doGetTransactionOutsGlobalIndices(transactionHash, outsGlobalIndices);
  callback(ec);
}

//it's always protected with mutex
std::error_code InProcessNode::doGetTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices) {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      return make_error_code(CryptoNote::error::NOT_INITIALIZED);
    }
  }

  try {
    bool r = core.get_tx_outputs_gindexs(transactionHash, outsGlobalIndices);
    if(!r) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }

  return std::error_code();
}

void InProcessNode::getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount,
    std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback)
{
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
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
  std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback)
{
  std::error_code ec = doGetRandomOutsByAmounts(std::move(amounts), outsCount, result);
  callback(ec);
}

//it's always protected with mutex
std::error_code InProcessNode::doGetRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount, std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result) {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      return make_error_code(CryptoNote::error::NOT_INITIALIZED);
    }
  }

  try {
    CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response res;
    CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request req;
    req.amounts = amounts;
    req.outs_count = outsCount;

    if(!core.get_random_outs_for_amounts(req, res)) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }

    result = std::move(res.outs);
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }

  return std::error_code();
}


void InProcessNode::relayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback)
{
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
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

void InProcessNode::relayTransactionAsync(const CryptoNote::Transaction& transaction, const Callback& callback) {
  std::error_code ec = doRelayTransaction(transaction);
  callback(ec);
}

//it's always protected with mutex
std::error_code InProcessNode::doRelayTransaction(const CryptoNote::Transaction& transaction) {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      return make_error_code(CryptoNote::error::NOT_INITIALIZED);
    }
  }

  try {
    CryptoNote::blobdata txBlob = CryptoNote::tx_to_blob(transaction);
    CryptoNote::tx_verification_context tvc = boost::value_initialized<CryptoNote::tx_verification_context>();

    if(!core.handle_incoming_tx(txBlob, tvc, false)) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }

    if(tvc.m_verifivation_failed) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }

    if(!tvc.m_should_be_relayed) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }

    CryptoNote::NOTIFY_NEW_TRANSACTIONS::request r;
    r.txs.push_back(txBlob);
    core.get_protocol()->relay_transactions(r);
  } catch (std::system_error& e) {
    return e.code();
  } catch (std::exception&) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }

  return std::error_code();
}

size_t InProcessNode::getPeerCount() const {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    }
  }

  return protocol.getPeerCount();
}

uint64_t InProcessNode::getLocalBlockCount() const {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    }
  }

  uint64_t lastIndex;
  crypto::hash ignore;

  core.get_blockchain_top(lastIndex, ignore);

  return lastIndex + 1;
}

uint64_t InProcessNode::getKnownBlockCount() const {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    }
  }

  return protocol.getObservedHeight();
}

uint64_t InProcessNode::getLastLocalBlockHeight() const {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    }
  }

  uint64_t height;
  crypto::hash ignore;

  if (!core.get_blockchain_top(height, ignore)) {
    throw std::system_error(make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR));
  }

  return height;
}

uint64_t InProcessNode::getLastKnownBlockHeight() const {
  {
    std::unique_lock<std::mutex> lock(mutex);
    if (state != INITIALIZED) {
      throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    }
  }

  return protocol.getObservedHeight() - 1;
}

void InProcessNode::peerCountUpdated(size_t count) {
  observerManager.notify(&INodeObserver::peerCountUpdated, count);
}

void InProcessNode::lastKnownBlockHeightUpdated(uint64_t height) {
  observerManager.notify(&INodeObserver::lastKnownBlockHeightUpdated, height);
}

uint64_t InProcessNode::getLastLocalBlockTimestamp() const {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::NOT_INITIALIZED));
  }
  lock.unlock();

  uint64_t ignore;
  crypto::hash hash;

  if (!core.get_blockchain_top(ignore, hash)) {
    throw std::system_error(make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR));
  }

  CryptoNote::Block block;
  if (!core.getBlockByHash(hash, block)) {
    throw std::system_error(make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR));
  }

  return block.timestamp;
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

void InProcessNode::blockchainSynchronized(uint64_t topHeight) {
  observerManager.notify(&INodeObserver::blockchainSynchronized, topHeight);
}

void InProcessNode::queryBlocks(std::list<crypto::hash>&& knownBlockIds, uint64_t timestamp,
    std::list<BlockCompleteEntry>& newBlocks, uint64_t& startHeight, const InProcessNode::Callback& callback)
{
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
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
  std::error_code ec = doQueryBlocks(std::move(knownBlockIds), timestamp, newBlocks, startHeight);
  callback(ec);
}

std::error_code InProcessNode::doQueryBlocks(std::list<crypto::hash>&& knownBlockIds, uint64_t timestamp,
    std::list<BlockCompleteEntry>& newBlocks, uint64_t& startHeight) {
  uint64_t currentHeight, fullOffset;
  std::list<CryptoNote::BlockFullInfo> entries;

  if (!core.queryBlocks(knownBlockIds, timestamp, startHeight, currentHeight, fullOffset, entries)) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
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

void InProcessNode::getPoolSymmetricDifference(std::vector<crypto::hash>&& knownPoolTxIds, crypto::hash knownBlockId, bool& isBcActual, std::vector<CryptoNote::Transaction>& newTxs,
  std::vector<crypto::hash>& deletedTxIds, const Callback& callback) {

  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
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

void InProcessNode::getPoolSymmetricDifferenceAsync(std::vector<crypto::hash>& known_pool_tx_ids, crypto::hash known_block_id, bool& is_bc_actual, std::vector<CryptoNote::Transaction>& new_txs,
  std::vector<crypto::hash>& deleted_tx_ids, const Callback& callback) {
  std::error_code ec = std::error_code();

  is_bc_actual = core.getPoolChanges(known_block_id, known_pool_tx_ids, new_txs, deleted_tx_ids);
  if (!is_bc_actual) {
    ec = make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }

  callback(ec);
}

void InProcessNode::getBlocks(const std::vector<uint64_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(
      static_cast<
        void(InProcessNode::*)(
          const std::vector<uint64_t>&, 
          std::vector<std::vector<BlockDetails>>&, 
          const Callback&
        )
      >(&InProcessNode::getBlocksAsync),
      this,
      std::cref(blockHeights),
      std::ref(blocks),
      callback
    )
  );
}

void InProcessNode::getBlocksAsync(const std::vector<uint64_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks, const Callback& callback) {
  std::error_code ec = doGetBlocks(blockHeights, blocks);
  callback(ec);
}

std::error_code InProcessNode::doGetBlocks(const std::vector<uint64_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks) {
  uint64_t topHeight = 0;
  crypto::hash topHash = boost::value_initialized<crypto::hash>();
  if (!core.get_blockchain_top(topHeight, topHash)) {
    return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
  }
  for (const uint64_t& height : blockHeights) {
    if (height > topHeight) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }
    crypto::hash hash = core.getBlockIdByHeight(height);
    Block block;
    if (!core.getBlockByHash(hash, block)) {
      return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
    }
    BlockDetails blockDetails;
    if (!blockchainExplorerDataBuilder.fillBlockDetails(block, blockDetails)) {
      return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
    }
    std::vector<BlockDetails> blocksOnSameHeight;
    blocksOnSameHeight.push_back(std::move(blockDetails));
    blocks.push_back(std::move(blocksOnSameHeight));
  }
  return std::error_code();
}

void InProcessNode::getBlocks(const std::vector<crypto::hash>& blockHashes, std::vector<BlockDetails>& blocks, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(
      static_cast<
        void(InProcessNode::*)(
          const std::vector<crypto::hash>&, 
          std::vector<BlockDetails>&, 
          const Callback&
        )
      >(&InProcessNode::getBlocksAsync),
      this,
      std::cref(blockHashes),
      std::ref(blocks),
      callback
    )
  );
}

void InProcessNode::getBlocksAsync(const std::vector<crypto::hash>& blockHashes, std::vector<BlockDetails>& blocks, const Callback& callback) {
  std::error_code ec = doGetBlocks(blockHashes, blocks);
  callback(ec);
}

std::error_code InProcessNode::doGetBlocks(const std::vector<crypto::hash>& blockHashes, std::vector<BlockDetails>& blocks) {
  for (const crypto::hash& hash : blockHashes) {
    Block block;
    if (!core.getBlockByHash(hash, block)) {
      return make_error_code(CryptoNote::error::REQUEST_ERROR);
    }
    BlockDetails blockDetails;
    if (!blockchainExplorerDataBuilder.fillBlockDetails(block, blockDetails)) {
      return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
    }
    blocks.push_back(std::move(blockDetails));
  }
  return std::error_code();
}

void InProcessNode::getTransactions(const std::vector<crypto::hash>& transactionHashes, std::vector<TransactionDetails>& transactions, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(
      &InProcessNode::getTransactionsAsync,
      this,
      std::cref(transactionHashes),
      std::ref(transactions),
      callback
    )
  );
}

void InProcessNode::getTransactionsAsync(const std::vector<crypto::hash>& transactionHashes, std::vector<TransactionDetails>& transactions, const Callback& callback) {
  std::error_code ec= doGetTransactions(transactionHashes, transactions);
  callback(ec);
}

std::error_code InProcessNode::doGetTransactions(const std::vector<crypto::hash>& transactionHashes, std::vector<TransactionDetails>& transactions) {
  std::list<Transaction> txs;
  std::list<crypto::hash> missed_txs;
  core.getTransactions(transactionHashes, txs, missed_txs, true);
  if (missed_txs.size() > 0) {
    return make_error_code(CryptoNote::error::REQUEST_ERROR);
  }
  for (const Transaction& tx : txs) {
    TransactionDetails transactionDetails;
    if (!blockchainExplorerDataBuilder.fillTransactionDetails(tx, transactionDetails)) {
      return make_error_code(CryptoNote::error::INTERNAL_NODE_ERROR);
    }
    transactions.push_back(std::move(transactionDetails));
  }
  return std::error_code();
}

void InProcessNode::isSynchronized(bool& syncStatus, const Callback& callback) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state != INITIALIZED) {
    lock.unlock();
    callback(make_error_code(CryptoNote::error::NOT_INITIALIZED));
    return;
  }

  ioService.post(
    std::bind(
      &InProcessNode::isSynchronizedAsync,
      this,
      std::ref(syncStatus),
      callback
    )
  );
}

void InProcessNode::isSynchronizedAsync(bool& syncStatus, const Callback& callback) {
  syncStatus = protocol.isSynchronized();
  callback(std::error_code());
}

} //namespace CryptoNote
