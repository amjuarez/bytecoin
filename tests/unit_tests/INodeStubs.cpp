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

#include "INodeStubs.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "wallet/WalletErrors.h"

#include <functional>
#include <thread>
#include <iterator>
#include <cassert>
#include <unordered_set>

#include "crypto/crypto.h"

namespace {

class ContextCounterHolder {
public:
  ContextCounterHolder(CryptoNote::WalletAsyncContextCounter& shutdowner) : m_shutdowner(shutdowner) {}
  ~ContextCounterHolder() { m_shutdowner.delAsyncContext(); }

private:
  CryptoNote::WalletAsyncContextCounter& m_shutdowner;
};

}


void INodeDummyStub::updateObservers() {
  observerManager.notify(&CryptoNote::INodeObserver::lastKnownBlockHeightUpdated, getLastKnownBlockHeight());
}

bool INodeDummyStub::addObserver(CryptoNote::INodeObserver* observer) {
  return observerManager.add(observer);
}

bool INodeDummyStub::removeObserver(CryptoNote::INodeObserver* observer) {
  return observerManager.remove(observer);
}

void INodeTrivialRefreshStub::getNewBlocks(std::list<crypto::hash>&& knownBlockIds, std::list<cryptonote::block_complete_entry>& newBlocks, uint64_t& startHeight, const Callback& callback)
{
  m_asyncCounter.addAsyncContext();
  std::thread task(std::bind(&INodeTrivialRefreshStub::doGetNewBlocks, this, std::move(knownBlockIds), std::ref(newBlocks), std::ref(startHeight), callback));
  task.detach();
}

void INodeTrivialRefreshStub::doGetNewBlocks(std::list<crypto::hash> knownBlockIds, std::list<cryptonote::block_complete_entry>& newBlocks, uint64_t& startHeight, const Callback& callback)
{
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_multiWalletLock);

  auto& blockchain = m_blockchainGenerator.getBlockchain();

  std::vector<cryptonote::Block>::iterator start = blockchain.end();

  for (const auto& id : knownBlockIds) {
    start = std::find_if(blockchain.begin(), blockchain.end(), 
      [&id](cryptonote::Block& block) { return get_block_hash(block) == id; });
    if (start != blockchain.end())
      break;
  }

  if (start == blockchain.end()) {
    callback(std::error_code());
    return;
  }

  m_lastHeight = std::distance(blockchain.begin(), start);
  startHeight = m_lastHeight; 

  for (; m_lastHeight < blockchain.size(); ++m_lastHeight)
  {
    cryptonote::block_complete_entry e;
    e.block = cryptonote::t_serializable_object_to_blob(blockchain[m_lastHeight]);

    for (auto hash : blockchain[m_lastHeight].txHashes)
    {
      cryptonote::Transaction tx;
      if (!m_blockchainGenerator.getTransactionByHash(hash, tx))
        continue;

      e.txs.push_back(t_serializable_object_to_blob(tx));
    }

    newBlocks.push_back(e);

    if (newBlocks.size() >= m_getMaxBlocks) {
      break;
    }
  }

  m_lastHeight = startHeight + newBlocks.size();
  // m_lastHeight = startHeight + blockchain.size() - 1;

  callback(std::error_code());
}

void INodeTrivialRefreshStub::getTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices, const Callback& callback)
{
  m_asyncCounter.addAsyncContext();
  std::unique_lock<std::mutex> lock(m_multiWalletLock);
  calls_getTransactionOutsGlobalIndices.push_back(transactionHash);
  std::thread task(&INodeTrivialRefreshStub::doGetTransactionOutsGlobalIndices, this, transactionHash, std::ref(outsGlobalIndices), callback);
  task.detach();
}

void INodeTrivialRefreshStub::doGetTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices, const Callback& callback)
{
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_multiWalletLock);

  cryptonote::Transaction tx;
  
  if (m_blockchainGenerator.getTransactionByHash(transactionHash, tx)) {
    outsGlobalIndices.resize(tx.vout.size());
  } else {
    outsGlobalIndices.resize(20); //random
  }

  callback(std::error_code());
}

void INodeTrivialRefreshStub::relayTransaction(const cryptonote::Transaction& transaction, const Callback& callback)
{
  m_asyncCounter.addAsyncContext();
  std::thread task(&INodeTrivialRefreshStub::doRelayTransaction, this, transaction, callback);
  task.detach();
}

void INodeTrivialRefreshStub::doRelayTransaction(const cryptonote::Transaction& transaction, const Callback& callback)
{
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_multiWalletLock);

  if (m_nextTxError)
  {
    m_nextTxError = false;
    callback(make_error_code(cryptonote::error::INTERNAL_WALLET_ERROR));
    return;
  }

  if (m_nextTxToPool) {
    m_nextTxToPool = false;
    m_blockchainGenerator.putTxToPool(transaction);
    callback(std::error_code());
    return;
  }

  m_blockchainGenerator.addTxToBlockchain(transaction);
  callback(std::error_code());
}

void INodeTrivialRefreshStub::getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount, std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback)
{
  m_asyncCounter.addAsyncContext();
  std::thread task(&INodeTrivialRefreshStub::doGetRandomOutsByAmounts, this, amounts, outsCount, std::ref(result), callback);
  task.detach();
}

void INodeTrivialRefreshStub::doGetRandomOutsByAmounts(std::vector<uint64_t> amounts, uint64_t outsCount, std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback)
{
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_multiWalletLock);
  for (uint64_t amount: amounts)
  {
    cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount out;
    out.amount = amount;

    for (uint64_t i = 0; i < outsCount; ++i)
    {
      crypto::public_key key;
      crypto::secret_key sk;
      generate_keys(key, sk);

      cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry e;
      e.global_amount_index = i;
      e.out_key = key;

      out.outs.push_back(e);
    }
  }

  callback(std::error_code());
}

void INodeTrivialRefreshStub::queryBlocks(std::list<crypto::hash>&& knownBlockIds, uint64_t timestamp, 
  std::list<CryptoNote::BlockCompleteEntry>& newBlocks, uint64_t& startHeight, const Callback& callback) {

  auto resultHolder = std::make_shared<std::list<cryptonote::block_complete_entry>>();

  getNewBlocks(std::move(knownBlockIds), *resultHolder, startHeight, [resultHolder, callback, &startHeight, &newBlocks](std::error_code ec)
  {
    if (ec == std::error_code()) {
      for (const auto& item : *resultHolder) {
        CryptoNote::BlockCompleteEntry entry;
        cryptonote::Block block;

        cryptonote::parse_and_validate_block_from_blob(item.block, block);

        entry.blockHash = cryptonote::get_block_hash(block);
        entry.block = item.block;
        entry.txs = std::move(item.txs);

        newBlocks.push_back(std::move(entry));
      }
    }
    callback(ec);  
  });
  
}


void INodeTrivialRefreshStub::startAlternativeChain(uint64_t height)
{
  std::vector<cryptonote::Block>& blockchain = m_blockchainGenerator.getBlockchain();

  assert(height < blockchain.size());
  //assert(height > m_lastHeight);

  auto it = blockchain.begin();
  std::advance(it, height);

  blockchain.erase(it, blockchain.end());

  m_lastHeight = height;
}

void INodeTrivialRefreshStub::setNextTransactionError()
{
  m_nextTxError = true;
}

void INodeTrivialRefreshStub::setNextTransactionToPool() {
  m_nextTxToPool = true;
}

void INodeTrivialRefreshStub::getPoolSymmetricDifference(std::vector<crypto::hash>&& known_pool_tx_ids, crypto::hash known_block_id, bool& is_bc_actual,
  std::vector<cryptonote::Transaction>& new_txs, std::vector<crypto::hash>& deleted_tx_ids, const Callback& callback)
{
  m_asyncCounter.addAsyncContext();
  std::thread task(
    std::bind(&INodeTrivialRefreshStub::doGetPoolSymmetricDifference, this,
      std::move(known_pool_tx_ids),
      known_block_id,
      std::ref(is_bc_actual),
      std::ref(new_txs),
      std::ref(deleted_tx_ids),
      callback
    )
  );
  task.detach();
}

void INodeTrivialRefreshStub::doGetPoolSymmetricDifference(std::vector<crypto::hash>& known_pool_tx_ids, crypto::hash known_block_id, bool& is_bc_actual,
  std::vector<cryptonote::Transaction>& new_txs, std::vector<crypto::hash>& deleted_tx_ids, const Callback& callback)
{
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_multiWalletLock);

  m_blockchainGenerator.getPoolSymmetricDifference(std::move(known_pool_tx_ids), known_block_id, is_bc_actual, new_txs, deleted_tx_ids);
  callback(std::error_code());
}

void INodeTrivialRefreshStub::includeTransactionsFromPoolToBlock() {
  m_blockchainGenerator.putTxPoolToBlockchain();
}

INodeTrivialRefreshStub::~INodeTrivialRefreshStub() {
  m_asyncCounter.waitAsyncContextsFinish();
}
