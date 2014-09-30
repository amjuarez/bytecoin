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
  TestBlockchainGenerator(const cryptonote::Currency& currency);

  std::vector<cryptonote::Block>& getBlockchain();
  void addGenesisBlock();
  void generateEmptyBlocks(size_t count);
  bool getBlockRewardForAddress(const cryptonote::AccountPublicAddress& address);
  void addTxToBlockchain(const cryptonote::Transaction& transaction);
  bool getTransactionByHash(const crypto::hash& hash, cryptonote::Transaction& tx);
  void startAlternativeChain(uint64_t height);

private:
  const cryptonote::Currency& m_currency;
  test_generator generator;
  cryptonote::account_base miner_acc;
  std::vector<cryptonote::Block> m_blockchain;
  std::unordered_map<crypto::hash, cryptonote::Transaction> m_txs;
  std::mutex mutex;
};
