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

#include "crypto/crypto.h"

void INodeTrivialRefreshStub::getNewBlocks(std::list<crypto::hash>&& knownBlockIds, std::list<cryptonote::block_complete_entry>& newBlocks, uint64_t& startHeight, const Callback& callback)
{
  incThreadsCount();
  std::thread task(&INodeTrivialRefreshStub::doGetNewBlocks, this, knownBlockIds, std::ref(newBlocks), std::ref(startHeight), callback);
  task.detach();
}

INodeTrivialRefreshStub::~INodeTrivialRefreshStub() {
  waitForThreadsFinish();
}

void INodeTrivialRefreshStub::doGetNewBlocks(std::list<crypto::hash> knownBlockIds, std::list<cryptonote::block_complete_entry>& newBlocks, uint64_t& startHeight, const Callback& callback)
{
  std::vector<cryptonote::Block> blockchain = m_blockchainGenerator.getBlockchain();

  startHeight = m_lastHeight;

  for (; m_lastHeight < blockchain.size(); ++m_lastHeight) {
    cryptonote::block_complete_entry e = makeCompleteBlockEntry(blockchain[m_lastHeight]);

    newBlocks.push_back(e);
  }

  m_lastHeight = blockchain.size() - 1;

  callback(std::error_code());

  decThreadsCount();
}

void INodeTrivialRefreshStub::decThreadsCount() {
  std::unique_lock<std::mutex> lock(m_mutex);
  --m_threadsCount;
  if (!m_threadsCount) {
    m_cv.notify_all();
  }
}

cryptonote::block_complete_entry INodeTrivialRefreshStub::makeCompleteBlockEntry(const cryptonote::Block &block) {
  cryptonote::block_complete_entry e;
  e.block = cryptonote::t_serializable_object_to_blob(block);

  for (auto hash : block.txHashes) {
    cryptonote::Transaction tx;
    if (!m_blockchainGenerator.getTransactionByHash(hash, tx))
      continue;

    e.txs.push_back(t_serializable_object_to_blob(tx));
  }

  return e;
}

void INodeTrivialRefreshStub::getTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices, const Callback& callback)
{
  incThreadsCount();
  std::thread task(&INodeTrivialRefreshStub::doGetTransactionOutsGlobalIndices, this, transactionHash, std::ref(outsGlobalIndices), callback);
  task.detach();
}

void INodeTrivialRefreshStub::incThreadsCount() {
  std::unique_lock<std::mutex> lock(m_mutex);
  ++m_threadsCount;
}

void INodeTrivialRefreshStub::doGetTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices, const Callback& callback)
{
  cryptonote::Transaction tx;
  if (m_blockchainGenerator.getTransactionByHash(transactionHash, tx)) {
    outsGlobalIndices.resize(tx.vout.size());
  } else {
    outsGlobalIndices.resize(20); //random
  }
  callback(std::error_code());
  decThreadsCount();
}

void INodeTrivialRefreshStub::relayTransaction(const cryptonote::Transaction& transaction, const Callback& callback)
{
  incThreadsCount();
  std::thread task(&INodeTrivialRefreshStub::doRelayTransaction, this, transaction, callback);
  task.detach();
}

void INodeTrivialRefreshStub::doRelayTransaction(const cryptonote::Transaction& transaction, const Callback& callback)
{
  if (m_nextTxError)
  {
    callback(make_error_code(cryptonote::error::INTERNAL_WALLET_ERROR));
    m_nextTxError = false;
    return;
  }
  m_blockchainGenerator.addTxToBlockchain(transaction);
  callback(std::error_code());
  decThreadsCount();
}

void INodeTrivialRefreshStub::getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount, std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback)
{
  incThreadsCount();
  std::thread task(&INodeTrivialRefreshStub::doGetRandomOutsByAmounts, this, amounts, outsCount, std::ref(result), callback);
  task.detach();
}

void INodeTrivialRefreshStub::doGetRandomOutsByAmounts(std::vector<uint64_t> amounts, uint64_t outsCount, std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback)
{
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
  decThreadsCount();
}

void INodeTrivialRefreshStub::startAlternativeChain(uint64_t height)
{
  assert(height > 1);
  m_blockchainGenerator.startAlternativeChain(height);
  m_lastHeight = height - 1;
}

void INodeTrivialRefreshStub::setNextTransactionError()
{
  m_nextTxError = true;
}

bool INodeTrivialRefreshStub::waitForThreadsFinish() {
  std::unique_lock<std::mutex> lock(m_mutex);

  return m_cv.wait_for(lock, std::chrono::seconds(3), [this] () { return m_threadsCount == 0; });
}