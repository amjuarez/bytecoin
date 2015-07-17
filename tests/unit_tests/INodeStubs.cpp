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

#include "INodeStubs.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "wallet/WalletErrors.h"

#include <functional>
#include <thread>
#include <iterator>
#include <cassert>
#include <unordered_set>

#include <system_error>

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

void INodeTrivialRefreshStub::getNewBlocks(std::list<crypto::hash>&& knownBlockIds, std::list<CryptoNote::block_complete_entry>& newBlocks, uint64_t& startHeight, const Callback& callback)
{
  m_asyncCounter.addAsyncContext();

  std::unique_lock<std::mutex> lock(m_multiWalletLock);
  auto blockchain = m_blockchainGenerator.getBlockchainCopy();
  lock.unlock();

  std::thread task(std::bind(&INodeTrivialRefreshStub::doGetNewBlocks, this, std::move(knownBlockIds), std::ref(newBlocks),
          std::ref(startHeight), std::move(blockchain), callback));
  task.detach();
}

void INodeTrivialRefreshStub::doGetNewBlocks(std::list<crypto::hash> knownBlockIds, std::list<CryptoNote::block_complete_entry>& newBlocks,
        uint64_t& startHeight, std::vector<CryptoNote::Block> blockchain, const Callback& callback)
{
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_multiWalletLock);

  std::vector<CryptoNote::Block>::iterator start = blockchain.end();

  for (const auto& id : knownBlockIds) {
    start = std::find_if(blockchain.begin(), blockchain.end(), 
      [&id](CryptoNote::Block& block) { return get_block_hash(block) == id; });
    if (start != blockchain.end())
      break;
  }

  if (start == blockchain.end()) {
    lock.unlock();
    callback(std::error_code());
    return;
  }

  m_lastHeight = std::distance(blockchain.begin(), start);
  startHeight = m_lastHeight; 

  for (; m_lastHeight < blockchain.size(); ++m_lastHeight)
  {
    CryptoNote::block_complete_entry e;
    e.block = CryptoNote::t_serializable_object_to_blob(blockchain[m_lastHeight]);

    for (auto hash : blockchain[m_lastHeight].txHashes)
    {
      CryptoNote::Transaction tx;
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

  lock.unlock();
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

  CryptoNote::Transaction tx;
  
  if (m_blockchainGenerator.getTransactionByHash(transactionHash, tx)) {
    outsGlobalIndices.resize(tx.vout.size());
  } else {
    outsGlobalIndices.resize(20); //random
  }

  lock.unlock();
  callback(std::error_code());
}

void INodeTrivialRefreshStub::relayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback)
{
  m_asyncCounter.addAsyncContext();
  std::thread task(&INodeTrivialRefreshStub::doRelayTransaction, this, transaction, callback);
  task.detach();
}

void INodeTrivialRefreshStub::doRelayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback)
{
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_multiWalletLock);

  if (m_nextTxError)
  {
    m_nextTxError = false;
    lock.unlock();
    callback(make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR));
    return;
  }

  if (m_nextTxToPool) {
    m_nextTxToPool = false;
    m_blockchainGenerator.putTxToPool(transaction);
    lock.unlock();
    callback(std::error_code());
    return;
  }

  m_blockchainGenerator.addTxToBlockchain(transaction);
  lock.unlock();
  callback(std::error_code());
}

void INodeTrivialRefreshStub::getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount, std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback)
{
  m_asyncCounter.addAsyncContext();
  std::thread task(&INodeTrivialRefreshStub::doGetRandomOutsByAmounts, this, amounts, outsCount, std::ref(result), callback);
  task.detach();
}

void INodeTrivialRefreshStub::doGetRandomOutsByAmounts(std::vector<uint64_t> amounts, uint64_t outsCount, std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback)
{
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_multiWalletLock);
  for (uint64_t amount: amounts)
  {
    CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount out;
    out.amount = amount;

    for (uint64_t i = 0; i < outsCount; ++i)
    {
      crypto::public_key key;
      crypto::secret_key sk;
      generate_keys(key, sk);

      CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry e;
      e.global_amount_index = i;
      e.out_key = key;

      out.outs.push_back(e);
    }

    result.push_back(std::move(out));
  }

  lock.unlock();
  callback(std::error_code());
}

void INodeTrivialRefreshStub::queryBlocks(std::list<crypto::hash>&& knownBlockIds, uint64_t timestamp, 
  std::list<CryptoNote::BlockCompleteEntry>& newBlocks, uint64_t& startHeight, const Callback& callback) {

  auto resultHolder = std::make_shared<std::list<CryptoNote::block_complete_entry>>();

  getNewBlocks(std::move(knownBlockIds), *resultHolder, startHeight, [resultHolder, callback, &startHeight, &newBlocks](std::error_code ec)
  {
    if (!ec) {
      for (const auto& item : *resultHolder) {
        CryptoNote::BlockCompleteEntry entry;
        CryptoNote::Block block;

        CryptoNote::parse_and_validate_block_from_blob(item.block, block);

        entry.blockHash = CryptoNote::get_block_hash(block);
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
  m_blockchainGenerator.cutBlockchain(height);
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
  std::vector<CryptoNote::Transaction>& new_txs, std::vector<crypto::hash>& deleted_tx_ids, const Callback& callback)
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
  std::vector<CryptoNote::Transaction>& new_txs, std::vector<crypto::hash>& deleted_tx_ids, const Callback& callback)
{
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_multiWalletLock);

  m_blockchainGenerator.getPoolSymmetricDifference(std::move(known_pool_tx_ids), known_block_id, is_bc_actual, new_txs, deleted_tx_ids);
  lock.unlock();
  callback(std::error_code());
}

void INodeTrivialRefreshStub::includeTransactionsFromPoolToBlock() {
  m_blockchainGenerator.putTxPoolToBlockchain();
}

INodeTrivialRefreshStub::~INodeTrivialRefreshStub() {
  m_asyncCounter.waitAsyncContextsFinish();
}

void INodeTrivialRefreshStub::getBlocks(const std::vector<uint64_t>& blockHeights, std::vector<std::vector<CryptoNote::BlockDetails>>& blocks, const Callback& callback) {
  m_asyncCounter.addAsyncContext();

  std::thread task(
    std::bind(
      static_cast<
        void(INodeTrivialRefreshStub::*)(
          const std::vector<uint64_t>&, 
          std::vector<std::vector<CryptoNote::BlockDetails>>&, 
          const Callback&
        )
      >(&INodeTrivialRefreshStub::doGetBlocks),
      this,
      std::cref(blockHeights),
      std::ref(blocks),
      callback
    )
  );
  task.detach();
}

void INodeTrivialRefreshStub::doGetBlocks(const std::vector<uint64_t>& blockHeights, std::vector<std::vector<CryptoNote::BlockDetails>>& blocks, const Callback& callback) {
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_multiWalletLock);

  for (const uint64_t& height: blockHeights) {
    if (m_blockchainGenerator.getBlockchain().size() <= height) {
      callback(std::error_code(EDOM, std::generic_category()));
      return;
    }
    CryptoNote::BlockDetails b = CryptoNote::BlockDetails();
    b.height = height;
    b.isOrphaned = false;
    crypto::hash hash = CryptoNote::get_block_hash(m_blockchainGenerator.getBlockchain()[height]);
    b.hash = reinterpret_cast<const std::array<uint8_t, 32>&>(hash);
    std::vector<CryptoNote::BlockDetails> v;
    v.push_back(b);
    blocks.push_back(v);
  }

  callback(std::error_code());
}

void INodeTrivialRefreshStub::getBlocks(const std::vector<crypto::hash>& blockHashes, std::vector<CryptoNote::BlockDetails>& blocks, const Callback& callback) {
  m_asyncCounter.addAsyncContext();

  std::thread task(
    std::bind(
      static_cast<
        void(INodeTrivialRefreshStub::*)(
          const std::vector<crypto::hash>&, 
          std::vector<CryptoNote::BlockDetails>&, 
          const Callback&
        )
      >(&INodeTrivialRefreshStub::doGetBlocks),
      this,
      std::cref(blockHashes),
      std::ref(blocks),
      callback
    )
  );
  task.detach();
}

void INodeTrivialRefreshStub::doGetBlocks(const std::vector<crypto::hash>& blockHashes, std::vector<CryptoNote::BlockDetails>& blocks, const Callback& callback) {
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_multiWalletLock);

  for (const crypto::hash& hash: blockHashes) {
    auto iter = std::find_if(
        m_blockchainGenerator.getBlockchain().begin(), 
        m_blockchainGenerator.getBlockchain().end(), 
        [&hash](const CryptoNote::Block& block) -> bool {
          return hash == CryptoNote::get_block_hash(block);
        }
    );
    if (iter == m_blockchainGenerator.getBlockchain().end()) {
      callback(std::error_code(EDOM, std::generic_category()));
      return;
    }
    CryptoNote::BlockDetails b = CryptoNote::BlockDetails();
    crypto::hash actualHash = CryptoNote::get_block_hash(*iter);
    b.hash = reinterpret_cast<const std::array<uint8_t, 32>&>(actualHash);
    b.isOrphaned = false;
    blocks.push_back(b);
  }

  callback(std::error_code());
}

void INodeTrivialRefreshStub::getTransactions(const std::vector<crypto::hash>& transactionHashes, std::vector<CryptoNote::TransactionDetails>& transactions, const Callback& callback) {
  m_asyncCounter.addAsyncContext();

  std::thread task(
    std::bind(
      &INodeTrivialRefreshStub::doGetTransactions,
      this,
      std::cref(transactionHashes),
      std::ref(transactions),
      callback
    )
  );
  task.detach();
}

void INodeTrivialRefreshStub::doGetTransactions(const std::vector<crypto::hash>& transactionHashes, std::vector<CryptoNote::TransactionDetails>& transactions, const Callback& callback) {
  ContextCounterHolder counterHolder(m_asyncCounter);
  std::unique_lock<std::mutex> lock(m_multiWalletLock);

  for (const crypto::hash& hash : transactionHashes) {
    CryptoNote::Transaction tx;
    CryptoNote::TransactionDetails txDetails = CryptoNote::TransactionDetails();
    if (m_blockchainGenerator.getTransactionByHash(hash, tx, false)) {
      crypto::hash actualHash = CryptoNote::get_transaction_hash(tx);
      txDetails.hash = reinterpret_cast<const std::array<uint8_t, 32>&>(actualHash);
      txDetails.inBlockchain = true;
    } else if (m_blockchainGenerator.getTransactionByHash(hash, tx, true)) {
      crypto::hash actualHash = CryptoNote::get_transaction_hash(tx);
      txDetails.hash = reinterpret_cast<const std::array<uint8_t, 32>&>(actualHash);
      txDetails.inBlockchain = false;
    } else {
      callback(std::error_code(EDOM, std::generic_category()));
      return;
    }
    
    transactions.push_back(txDetails);
  }

  callback(std::error_code());
}

void INodeTrivialRefreshStub::isSynchronized(bool& syncStatus, const Callback& callback) {
  //m_asyncCounter.addAsyncContext();
  syncStatus = m_synchronized;
  callback(std::error_code());
}

void INodeTrivialRefreshStub::setSynchronizedStatus(bool status) {
  m_synchronized = status;
  if (m_synchronized) {
    observerManager.notify(&CryptoNote::INodeObserver::blockchainSynchronized, getLastLocalBlockHeight());
  }
}

void INodeTrivialRefreshStub::sendPoolChanged() {
  observerManager.notify(&CryptoNote::INodeObserver::poolChanged);
}

void INodeTrivialRefreshStub::sendLocalBlockchainUpdated(){
  observerManager.notify(&CryptoNote::INodeObserver::localBlockchainUpdated, getLastLocalBlockHeight());
}
