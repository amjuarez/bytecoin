// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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

  void generate(const cryptonote::account_public_address& address, cryptonote::transaction& tx)
  {
    cryptonote::tx_destination_entry destination(this->m_source_amount, address);
    std::vector<cryptonote::tx_destination_entry> destinations;
    destinations.push_back(destination);

    cryptonote::construct_tx(this->m_miners[this->real_source_idx].get_keys(), this->m_sources, destinations, std::vector<uint8_t>(), tx, 0);
  }
};


TestBlockchainGenerator::TestBlockchainGenerator()
{
  miner_acc.generate();
  addGenesisBlock();
}

std::vector<cryptonote::block>& TestBlockchainGenerator::getBlockchain()
{
  return m_blockchain;
}

bool TestBlockchainGenerator::getTransactionByHash(const crypto::hash& hash, cryptonote::transaction& tx)
{
  auto it = m_txs.find(hash);
  if (it == m_txs.end())
    return false;

  tx = it->second;
  return true;
}

void TestBlockchainGenerator::addGenesisBlock()
{
  cryptonote::block genesis;
  uint64_t timestamp = time(NULL);

  generator.construct_block(genesis, miner_acc, timestamp);
  m_blockchain.push_back(genesis);
}

void TestBlockchainGenerator::generateEmptyBlocks(size_t count)
{
  addGenesisBlock();

  for (size_t i = 0; i < count; ++i)
  {
    cryptonote::block& prev_block = m_blockchain.back();
    cryptonote::block block;
    generator.construct_block(block, prev_block, miner_acc);
    m_blockchain.push_back(block);
  }
}

void TestBlockchainGenerator::addTxToBlockchain(const cryptonote::transaction& transaction)
{
  crypto::hash txHash = cryptonote::get_transaction_hash(transaction);
  m_txs[txHash] = transaction;

  std::list<cryptonote::transaction> txs;
  txs.push_back(transaction);

  cryptonote::block& prev_block = m_blockchain.back();
  cryptonote::block block;

  generator.construct_block(block, prev_block, miner_acc, txs);
  m_blockchain.push_back(block);
}

bool TestBlockchainGenerator::getBlockRewardForAddress(const cryptonote::account_public_address& address)
{
  TransactionForAddressCreator creator;
  if (!creator.init())
    return false;

  cryptonote::transaction tx;
  creator.generate(address, tx);

  crypto::hash txHash = cryptonote::get_transaction_hash(tx);
  m_txs[txHash] = tx;

  std::list<cryptonote::transaction> txs;
  txs.push_back(tx);

  cryptonote::block& prev_block = m_blockchain.back();
  cryptonote::block block;

  generator.construct_block(block, prev_block, miner_acc, txs);
  m_blockchain.push_back(block);

  return true;
}

