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

#include <time.h>
#include "TestBlockchainGenerator.h"

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
        [&](uint64_t a_dust) { destinations.push_back(cryptonote::tx_destination_entry(a_dust, address)); } );

    cryptonote::construct_tx(this->m_miners[this->real_source_idx].get_keys(), this->m_sources, destinations, std::vector<uint8_t>(), tx, 0);

  }
};


TestBlockchainGenerator::TestBlockchainGenerator(const cryptonote::Currency& currency) :
  m_currency(currency),
  generator(currency)
{
  miner_acc.generate();
  addGenesisBlock();
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

void TestBlockchainGenerator::addGenesisBlock()
{
  cryptonote::Block genesis;
  uint64_t timestamp = time(NULL);

  generator.constructBlock(genesis, miner_acc, timestamp);
  m_blockchain.push_back(genesis);
}

void TestBlockchainGenerator::generateEmptyBlocks(size_t count)
{
  addGenesisBlock();

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

  crypto::hash txHash = cryptonote::get_transaction_hash(tx);
  m_txs[txHash] = tx;

  std::list<cryptonote::Transaction> txs;
  txs.push_back(tx);

  cryptonote::Block& prev_block = m_blockchain.back();
  cryptonote::Block block;

  generator.constructBlock(block, prev_block, miner_acc, txs);
  m_blockchain.push_back(block);

  return true;
}

void TestBlockchainGenerator::startAlternativeChain(uint64_t height) {
  std::unique_lock<std::mutex> lock(mutex);

  auto it = m_blockchain.begin();
  std::advance(it, height);

  m_blockchain.erase(it, m_blockchain.end());
}