// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <vector>
#include <unordered_map>

#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/BlockchainIndices.h"
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
  bool getTransactionByHash(const Crypto::Hash& hash, CryptoNote::Transaction& tx, bool checkTxPool = false);
  const CryptoNote::AccountBase& getMinerAccount() const;
  bool generateFromBaseTx(const CryptoNote::AccountBase& address);

  void putTxToPool(const CryptoNote::Transaction& tx);
  void getPoolSymmetricDifference(std::vector<Crypto::Hash>&& known_pool_tx_ids, Crypto::Hash known_block_id, bool& is_bc_actual,
    std::vector<CryptoNote::Transaction>& new_txs, std::vector<Crypto::Hash>& deleted_tx_ids);
  void putTxPoolToBlockchain();
  void clearTxPool();

  void cutBlockchain(uint32_t height);

  bool addOrphan(const Crypto::Hash& hash, uint32_t height);
  bool getGeneratedTransactionsNumber(uint32_t height, uint64_t& generatedTransactions);
  bool getOrphanBlockIdsByHeight(uint32_t height, std::vector<Crypto::Hash>& blockHashes);
  bool getBlockIdsByTimestamp(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<Crypto::Hash>& hashes, uint32_t& blocksNumberWithinTimestamps);
  bool getPoolTransactionIdsByTimestamp(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<Crypto::Hash>& hashes, uint64_t& transactionsNumberWithinTimestamps);
  bool getTransactionIdsByPaymentId(const Crypto::Hash& paymentId, std::vector<Crypto::Hash>& transactionHashes);

  bool getTransactionGlobalIndexesByHash(const Crypto::Hash& transactionHash, std::vector<uint32_t>& globalIndexes);
  bool getMultisignatureOutputByGlobalIndex(uint64_t amount, uint32_t globalIndex, CryptoNote::MultisignatureOutput& out);
  void setMinerAccount(const CryptoNote::AccountBase& account);

private:
  struct MultisignatureOutEntry {
    Crypto::Hash transactionHash;
    uint16_t indexOut;
  };

  struct KeyOutEntry {
    Crypto::Hash transactionHash;
    uint16_t indexOut;
  };
  
  void addGenesisBlock();
  void addMiningBlock();

  const CryptoNote::Currency& m_currency;
  test_generator generator;
  CryptoNote::AccountBase miner_acc;
  std::vector<CryptoNote::Block> m_blockchain;
  std::unordered_map<Crypto::Hash, CryptoNote::Transaction> m_txs;
  std::unordered_map<Crypto::Hash, std::vector<uint32_t>> transactionGlobalOuts;
  std::unordered_map<uint64_t, std::vector<MultisignatureOutEntry>> multisignatureOutsIndex;
  std::unordered_map<uint64_t, std::vector<KeyOutEntry>> keyOutsIndex;

  std::unordered_map<Crypto::Hash, CryptoNote::Transaction> m_txPool;
  mutable std::mutex m_mutex;

  CryptoNote::PaymentIdIndex m_paymentIdIndex;
  CryptoNote::TimestampTransactionsIndex m_timestampIndex;
  CryptoNote::GeneratedTransactionsIndex m_generatedTransactionsIndex;
  CryptoNote::OrphanBlocksIndex m_orthanBlocksIndex;

  void addToBlockchain(const CryptoNote::Transaction& tx);
  void addToBlockchain(const std::vector<CryptoNote::Transaction>& txs);
  void addToBlockchain(const std::vector<CryptoNote::Transaction>& txs, const CryptoNote::AccountBase& minerAddress);
  void addTx(const CryptoNote::Transaction& tx);

  bool doGenerateTransactionsInOneBlock(CryptoNote::AccountPublicAddress const &address, size_t n);
};
