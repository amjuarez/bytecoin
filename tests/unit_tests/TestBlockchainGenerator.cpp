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

#include "TestBlockchainGenerator.h"

#include <time.h>
#include <unordered_set>

#include "cryptonote_core/cryptonote_format_utils.h"

#include "../performance_tests/multi_tx_test_base.h"

using namespace CryptoNote;

class TransactionForAddressCreator : public multi_tx_test_base<5>
{
  typedef multi_tx_test_base<5> base_class;
public:
  TransactionForAddressCreator() {}

  bool init()
  {
    return base_class::init();
  }

  void generate(const AccountPublicAddress& address, Transaction& tx)
  {
    std::vector<CryptoNote::tx_destination_entry> destinations;

    CryptoNote::decompose_amount_into_digits(this->m_source_amount, 0,
      [&](uint64_t chunk) { destinations.push_back(CryptoNote::tx_destination_entry(chunk, address)); },
      [&](uint64_t a_dust) { destinations.push_back(CryptoNote::tx_destination_entry(a_dust, address)); });

    CryptoNote::construct_tx(this->m_miners[this->real_source_idx].get_keys(), this->m_sources, destinations, std::vector<uint8_t>(), tx, 0, m_logger);
  }

  void generateSingleOutputTx(const AccountPublicAddress& address, uint64_t amount, Transaction& tx) {
    std::vector<tx_destination_entry> destinations;
    destinations.push_back(tx_destination_entry(amount, address));
    construct_tx(this->m_miners[this->real_source_idx].get_keys(), this->m_sources, destinations, std::vector<uint8_t>(), tx, 0, m_logger);
  }
};


TestBlockchainGenerator::TestBlockchainGenerator(const CryptoNote::Currency& currency) :
  m_currency(currency),
  generator(currency)
{
  std::unique_lock<std::mutex> lock(m_mutex);

  miner_acc.generate();
  addGenesisBlock();
  addMiningBlock();
}

std::vector<CryptoNote::Block>& TestBlockchainGenerator::getBlockchain()
{
  std::unique_lock<std::mutex> lock(m_mutex);
  return m_blockchain;
}

std::vector<CryptoNote::Block> TestBlockchainGenerator::getBlockchainCopy() {
  std::unique_lock<std::mutex> lock(m_mutex);

  std::vector<CryptoNote::Block> blockchain(m_blockchain);
  return blockchain;
}

bool TestBlockchainGenerator::getTransactionByHash(const crypto::hash& hash, CryptoNote::Transaction& tx, bool checkTxPool)
{
  std::unique_lock<std::mutex> lock(m_mutex);

  auto it = m_txs.find(hash);
  if (it != m_txs.end()) {
    tx = it->second;
    return true;
  } else if (checkTxPool) {
    auto poolIt = m_txPool.find(hash);
    if (poolIt != m_txPool.end()) {
      tx = poolIt->second;
      return true;
    }
  }

  return false;
}

const CryptoNote::account_base& TestBlockchainGenerator::getMinerAccount() const {
  std::unique_lock<std::mutex> lock(m_mutex);
  return miner_acc;
}

void TestBlockchainGenerator::addGenesisBlock() {
  std::vector<size_t> bsizes;
  generator.addBlock(m_currency.genesisBlock(), 0, 0, bsizes, 0);
  m_blockchain.push_back(m_currency.genesisBlock());
}

void TestBlockchainGenerator::addMiningBlock() {
  CryptoNote::Block block;
  uint64_t timestamp = time(NULL);
  generator.constructBlock(block, miner_acc, timestamp);
  m_blockchain.push_back(block);
}

void TestBlockchainGenerator::generateEmptyBlocks(size_t count)
{
  std::unique_lock<std::mutex> lock(m_mutex);

  for (size_t i = 0; i < count; ++i)
  {
    CryptoNote::Block& prev_block = m_blockchain.back();
    CryptoNote::Block block;
    generator.constructBlock(block, prev_block, miner_acc);
    m_blockchain.push_back(block);
  }
}

void TestBlockchainGenerator::addTxToBlockchain(const CryptoNote::Transaction& transaction)
{
  std::unique_lock<std::mutex> lock(m_mutex);

  crypto::hash txHash = CryptoNote::get_transaction_hash(transaction);
  m_txs[txHash] = transaction;

  std::list<CryptoNote::Transaction> txs;
  txs.push_back(transaction);

  CryptoNote::Block& prev_block = m_blockchain.back();
  CryptoNote::Block block;

  generator.constructBlock(block, prev_block, miner_acc, txs);
  m_blockchain.push_back(block);
}

bool TestBlockchainGenerator::getBlockRewardForAddress(const CryptoNote::AccountPublicAddress& address)
{
  std::unique_lock<std::mutex> lock(m_mutex);

  doGenerateTransactionsInOneBlock(address, 1);
  return true;
}

bool TestBlockchainGenerator::generateTransactionsInOneBlock(const CryptoNote::AccountPublicAddress& address, size_t n) {
  std::unique_lock<std::mutex> lock(m_mutex);

  return doGenerateTransactionsInOneBlock(address, n);
}

bool TestBlockchainGenerator::doGenerateTransactionsInOneBlock(const AccountPublicAddress &address, size_t n) {
  assert(n > 0);

  TransactionForAddressCreator creator;
  if (!creator.init())
    return false;

  std::vector<Transaction> txs;
  for (size_t i = 0; i < n; ++i) {
    Transaction tx;
    creator.generate(address, tx);
    tx.unlockTime = 10; //default unlock time for coinbase transactions
    txs.push_back(tx);
  }

  addToBlockchain(txs);

  return true;
}

bool TestBlockchainGenerator::getSingleOutputTransaction(const CryptoNote::AccountPublicAddress& address, uint64_t amount) {
  std::unique_lock<std::mutex> lock(m_mutex);

  TransactionForAddressCreator creator;
  if (!creator.init())
    return false;

  CryptoNote::Transaction tx;
  creator.generateSingleOutputTx(address, amount, tx);

  addToBlockchain(tx);

  return true;
}

void TestBlockchainGenerator::addToBlockchain(const CryptoNote::Transaction& tx) {
  addToBlockchain(std::vector<CryptoNote::Transaction> {tx});
}

void TestBlockchainGenerator::addToBlockchain(const std::vector<CryptoNote::Transaction>& txs) {
  std::list<CryptoNote::Transaction> txsToBlock;

  for (const auto& tx: txs) {
    crypto::hash txHash = get_transaction_hash(tx);
    m_txs[txHash] = tx;

    txsToBlock.push_back(tx);
  }

  CryptoNote::Block& prev_block = m_blockchain.back();
  CryptoNote::Block block;

  generator.constructBlock(block, prev_block, miner_acc, txsToBlock);
  m_blockchain.push_back(block);
}

void TestBlockchainGenerator::getPoolSymmetricDifference(std::vector<crypto::hash>&& known_pool_tx_ids, crypto::hash known_block_id, bool& is_bc_actual,
  std::vector<CryptoNote::Transaction>& new_txs, std::vector<crypto::hash>& deleted_tx_ids)
{
  std::unique_lock<std::mutex> lock(m_mutex);

  if (known_block_id != CryptoNote::get_block_hash(m_blockchain.back())) {
    is_bc_actual = false;
    return;
  }

  is_bc_actual = true;

  std::unordered_set<crypto::hash> txIds;
  for (const auto& kv : m_txPool) {
    txIds.insert(kv.first);
  }

  std::unordered_set<crypto::hash> known_set(known_pool_tx_ids.begin(), known_pool_tx_ids.end());
  for (auto it = txIds.begin(), e = txIds.end(); it != e;) {
    auto known_it = known_set.find(*it);
    if (known_it != known_set.end()) {
      known_set.erase(known_it);
      it = txIds.erase(it);
    }
    else {
      new_txs.push_back(m_txPool[*it]);
      ++it;
    }
  }

  deleted_tx_ids.assign(known_set.begin(), known_set.end());
}

void TestBlockchainGenerator::putTxToPool(const CryptoNote::Transaction& tx) {
  std::unique_lock<std::mutex> lock(m_mutex);

  crypto::hash txHash = CryptoNote::get_transaction_hash(tx);
  m_txPool[txHash] = tx;
}

void TestBlockchainGenerator::putTxPoolToBlockchain() {
  std::unique_lock<std::mutex> lock(m_mutex);

  std::list<CryptoNote::Transaction> txs;

  for (const auto& kv: m_txPool) {
    m_txs[kv.first] = kv.second;
    txs.push_back(kv.second);
  }

  CryptoNote::Block& prev_block = m_blockchain.back();
  CryptoNote::Block block;

  generator.constructBlock(block, prev_block, miner_acc, txs);
  m_blockchain.push_back(block);
  m_txPool.clear();
}

void TestBlockchainGenerator::clearTxPool() {
  std::unique_lock<std::mutex> lock(m_mutex);

  m_txPool.clear();
}

void TestBlockchainGenerator::cutBlockchain(size_t height) {
  std::unique_lock<std::mutex> lock(m_mutex);

  assert(height < m_blockchain.size());
  //assert(height > m_lastHeight);

  auto it = m_blockchain.begin();
  std::advance(it, height);

  m_blockchain.erase(it, m_blockchain.end());

  //TODO: delete transactions from m_txs
}
