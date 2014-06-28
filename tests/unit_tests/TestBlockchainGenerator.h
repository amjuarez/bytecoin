// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "../core_tests/chaingen.h"
#include <vector>
#include <unordered_map>

#include "cryptonote_core/account.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "crypto/hash.h"

class TestBlockchainGenerator
{
public:
  TestBlockchainGenerator();

  std::vector<cryptonote::block>& getBlockchain();
  void addGenesisBlock();
  void generateEmptyBlocks(size_t count);
  bool getBlockRewardForAddress(const cryptonote::account_public_address& address);
  void addTxToBlockchain(const cryptonote::transaction& transaction);
  bool getTransactionByHash(const crypto::hash& hash, cryptonote::transaction& tx);

private:
  test_generator generator;
  cryptonote::account_base miner_acc;
  std::vector<cryptonote::block> m_blockchain;
  std::unordered_map<crypto::hash, cryptonote::transaction> m_txs;
};
