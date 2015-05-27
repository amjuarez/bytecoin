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

#pragma once

#include <vector>
#include <unordered_map>

#include "cryptonote_core/account.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/Currency.h"
#include "crypto/hash.h"

#include "../TestGenerator/TestGenerator.h"

class TestBlockchainGenerator
{
public:
  TestBlockchainGenerator(const CryptoNote::Currency& currency);

  //TODO: get rid of this method
  std::vector<CryptoNote::Block>& getBlockchain();
  std::vector<CryptoNote::Block> getBlockchainCopy();
  void generateEmptyBlocks(size_t count);
  bool getBlockRewardForAddress(const CryptoNote::AccountPublicAddress& address);
  bool generateTransactionsInOneBlock(const CryptoNote::AccountPublicAddress& address, size_t n);
  bool getSingleOutputTransaction(const CryptoNote::AccountPublicAddress& address, uint64_t amount);
  void addTxToBlockchain(const CryptoNote::Transaction& transaction);
  bool getTransactionByHash(const crypto::hash& hash, CryptoNote::Transaction& tx, bool checkTxPool = false);
  const CryptoNote::account_base& getMinerAccount() const;

  void putTxToPool(const CryptoNote::Transaction& tx);
  void getPoolSymmetricDifference(std::vector<crypto::hash>&& known_pool_tx_ids, crypto::hash known_block_id, bool& is_bc_actual,
    std::vector<CryptoNote::Transaction>& new_txs, std::vector<crypto::hash>& deleted_tx_ids);
  void putTxPoolToBlockchain();
  void clearTxPool();

  void cutBlockchain(size_t height);

private:
  
  void addGenesisBlock();
  void addMiningBlock();

  const CryptoNote::Currency& m_currency;
  test_generator generator;
  CryptoNote::account_base miner_acc;
  std::vector<CryptoNote::Block> m_blockchain;
  std::unordered_map<crypto::hash, CryptoNote::Transaction> m_txs;
  std::unordered_map<crypto::hash, CryptoNote::Transaction> m_txPool;
  mutable std::mutex m_mutex;

  void addToBlockchain(const CryptoNote::Transaction& tx);
  void addToBlockchain(const std::vector<CryptoNote::Transaction>& txs);

  bool doGenerateTransactionsInOneBlock(CryptoNote::AccountPublicAddress const &address, size_t n);
};
