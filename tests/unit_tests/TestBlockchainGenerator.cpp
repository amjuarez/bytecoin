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

#include "TestBlockchainGenerator.h"

#include <time.h>
#include <unordered_set>

#include "cryptonote_core/cryptonote_format_utils.h"

#include "../performance_tests/multi_tx_test_base.h"

class TransactionForAddressCreator : public multi_tx_test_base<5>
{
  typedef multi_tx_test_base<5> base_class;
public:
  TransactionForAddressCreator() {}

  bool init()
  {
    return base_class::init();
  }

  void generate(const cryptonote::AccountPublicAddress& address, cryptonote::Transaction& tx)
  {
    std::vector<cryptonote::tx_destination_entry> destinations;

    cryptonote::decompose_amount_into_digits(this->m_source_amount, 0,
      [&](uint64_t chunk) { destinations.push_back(cryptonote::tx_destination_entry(chunk, address)); },
      [&](uint64_t a_dust) { destinations.push_back(cryptonote::tx_destination_entry(a_dust, address)); });

    cryptonote::construct_tx(this->m_miners[this->real_source_idx].get_keys(), this->m_sources, destinations, std::vector<uint8_t>(), tx, 0);
  }

  void generateSingleOutputTx(const cryptonote::AccountPublicAddress& address, uint64_t amount, cryptonote::Transaction& tx) {
    std::vector<cryptonote::tx_destination_entry> destinations;

    destinations.push_back(cryptonote::tx_destination_entry(amount, address));

    cryptonote::construct_tx(this->m_miners[this->real_source_idx].get_keys(), this->m_sources, destinations, std::vector<uint8_t>(), tx, 0);
  }
};


TestBlockchainGenerator::TestBlockchainGenerator(const cryptonote::Currency& currency) :
  m_currency(currency),
  generator(currency)
{
  miner_acc.generate();
  addGenesisBlock();
  addMiningBlock();
}

std::vector<cryptonote::Block>& TestBlockchainGenerator::getBlockchain()
{
  return m_blockchain;
}

bool TestBlockchainGenerator::getTransactionByHash(const crypto::hash& hash, cryptonote::Transaction& tx)
{
  auto it = m_txs.find(hash);
  if (it == m_txs.end())
    return false;

  tx = it->second;
  return true;
}

void TestBlockchainGenerator::addGenesisBlock() {
  std::vector<size_t> bsizes;
  generator.addBlock(m_currency.genesisBlock(), 0, 0, bsizes, 0);
  m_blockchain.push_back(m_currency.genesisBlock());
}

void TestBlockchainGenerator::addMiningBlock() {
  cryptonote::Block block;
  uint64_t timestamp = time(NULL);
  generator.constructBlock(block, miner_acc, timestamp);
  m_blockchain.push_back(block);
}

void TestBlockchainGenerator::generateEmptyBlocks(size_t count)
{
  for (size_t i = 0; i < count; ++i)
  {
    cryptonote::Block& prev_block = m_blockchain.back();
    cryptonote::Block block;
    generator.constructBlock(block, prev_block, miner_acc);
    m_blockchain.push_back(block);
  }
}

void TestBlockchainGenerator::addTxToBlockchain(const cryptonote::Transaction& transaction)
{
  crypto::hash txHash = cryptonote::get_transaction_hash(transaction);
  m_txs[txHash] = transaction;

  std::list<cryptonote::Transaction> txs;
  txs.push_back(transaction);

  cryptonote::Block& prev_block = m_blockchain.back();
  cryptonote::Block block;

  generator.constructBlock(block, prev_block, miner_acc, txs);
  m_blockchain.push_back(block);
}

bool TestBlockchainGenerator::getBlockRewardForAddress(const cryptonote::AccountPublicAddress& address)
{
  TransactionForAddressCreator creator;
  if (!creator.init())
    return false;

  cryptonote::Transaction tx;
  creator.generate(address, tx);
  tx.unlockTime = 10; //default unlock time for coinbase transactions

  addToBlockchain(tx);

  return true;
}

bool TestBlockchainGenerator::getSingleOutputTransaction(const cryptonote::AccountPublicAddress& address, uint64_t amount) {
  TransactionForAddressCreator creator;
  if (!creator.init())
    return false;

  cryptonote::Transaction tx;
  creator.generateSingleOutputTx(address, amount, tx);

  addToBlockchain(tx);

  return true;
}

void TestBlockchainGenerator::addToBlockchain(const cryptonote::Transaction& tx) {
  crypto::hash txHash = get_transaction_hash(tx);
  m_txs[txHash] = tx;

  std::list<cryptonote::Transaction> txs;
  txs.push_back(tx);

  cryptonote::Block& prev_block = m_blockchain.back();
  cryptonote::Block block;

  generator.constructBlock(block, prev_block, miner_acc, txs);
  m_blockchain.push_back(block);
}

void TestBlockchainGenerator::getPoolSymmetricDifference(std::vector<crypto::hash>&& known_pool_tx_ids, crypto::hash known_block_id, bool& is_bc_actual,
  std::vector<cryptonote::Transaction>& new_txs, std::vector<crypto::hash>& deleted_tx_ids)
{
  if (known_block_id != cryptonote::get_block_hash(m_blockchain.back())) {
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

void TestBlockchainGenerator::putTxToPool(const cryptonote::Transaction& tx) {
  crypto::hash txHash = cryptonote::get_transaction_hash(tx);
  m_txPool[txHash] = tx;
}

void TestBlockchainGenerator::putTxPoolToBlockchain() {
  std::list<cryptonote::Transaction> txs;

  for (const auto& kv: m_txPool) {
    m_txs[kv.first] = kv.second;
    txs.push_back(kv.second);
  }

  cryptonote::Block& prev_block = m_blockchain.back();
  cryptonote::Block block;

  generator.constructBlock(block, prev_block, miner_acc, txs);
  m_blockchain.push_back(block);
  m_txPool.clear();
}

void TestBlockchainGenerator::clearTxPool() {
  m_txPool.clear();
}
