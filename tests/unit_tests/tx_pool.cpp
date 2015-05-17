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

#include "gtest/gtest.h"

#include <algorithm>

#include "cryptonote_core/account.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/Currency.h"
#include "cryptonote_core/tx_pool.h"

using namespace cryptonote;
using namespace CryptoNote;

class TransactionValidator : public CryptoNote::ITransactionValidator {
  virtual bool checkTransactionInputs(const cryptonote::Transaction& tx, BlockInfo& maxUsedBlock) {
    return true;
  }

  virtual bool checkTransactionInputs(const cryptonote::Transaction& tx, BlockInfo& maxUsedBlock, BlockInfo& lastFailed) {
    return true;
  }

  virtual bool haveSpentKeyImages(const cryptonote::Transaction& tx) {
    return false;
  }
};

class FakeTimeProvider : public ITimeProvider {
public:
  FakeTimeProvider(time_t currentTime = time(nullptr))
    : timeNow(currentTime) {}

  time_t timeNow;
  virtual time_t now() { return timeNow; }
};


class TestTransactionGenerator {

public:

  TestTransactionGenerator(const cryptonote::Currency& currency, size_t ringSize) :
    m_currency(currency),
    m_ringSize(ringSize),
    m_miners(ringSize), 
    m_miner_txs(ringSize), 
    m_public_keys(ringSize), 
    m_public_key_ptrs(ringSize) 
  {
    rv_acc.generate();
  }

  bool createSources() {

    size_t real_source_idx = m_ringSize / 2;

    std::vector<tx_source_entry::output_entry> output_entries;
    for (size_t i = 0; i < m_ringSize; ++i)
    {
      m_miners[i].generate();

      if (!m_currency.constructMinerTx(0, 0, 0, 2, 0, m_miners[i].get_keys().m_account_address, m_miner_txs[i])) {
        return false;
      }

      TransactionOutputToKey tx_out = boost::get<TransactionOutputToKey>(m_miner_txs[i].vout[0].target);
      output_entries.push_back(std::make_pair(i, tx_out.key));
      m_public_keys[i] = tx_out.key;
      m_public_key_ptrs[i] = &m_public_keys[i];
    }

    m_source_amount = m_miner_txs[0].vout[0].amount;

    tx_source_entry source_entry;
    source_entry.amount = m_source_amount;
    source_entry.real_out_tx_key = get_tx_pub_key_from_extra(m_miner_txs[real_source_idx]);
    source_entry.real_output_in_tx_index = 0;
    source_entry.outputs.swap(output_entries);
    source_entry.real_output = real_source_idx;

    m_sources.push_back(source_entry);

    m_realSenderKeys = m_miners[real_source_idx].get_keys();

    return true;
  }

  void construct(uint64_t amount, uint64_t fee, size_t outputs, Transaction& tx) {

    std::vector<tx_destination_entry> destinations;
    uint64_t amountPerOut = (amount - fee) / outputs;

    for (size_t i = 0; i < outputs; ++i) {
      destinations.push_back(tx_destination_entry(amountPerOut, rv_acc.get_keys().m_account_address));
    }

    construct_tx(m_realSenderKeys, m_sources, destinations, std::vector<uint8_t>(), tx, 0);
  }

  std::vector<account_base> m_miners;
  std::vector<Transaction> m_miner_txs;
  std::vector<tx_source_entry> m_sources;
  std::vector<crypto::public_key> m_public_keys;
  std::vector<const crypto::public_key*> m_public_key_ptrs;

  const cryptonote::Currency& m_currency;
  const size_t m_ringSize;
  account_keys m_realSenderKeys;
  uint64_t m_source_amount;
  account_base rv_acc;
};



namespace
{
  static const size_t textMaxCumulativeSize = std::numeric_limits<size_t>::max();

  void GenerateTransaction(const cryptonote::Currency& currency, Transaction& tx, uint64_t fee, size_t outputs) {
    TestTransactionGenerator txGenerator(currency, 1);
    txGenerator.createSources();
    txGenerator.construct(txGenerator.m_source_amount, fee, outputs, tx);
  }
  
  template <typename Validator, typename TimeProvider>
  class TestPool : public tx_memory_pool {
  public:

    Validator validator;
    TimeProvider timeProvider;

    TestPool(const cryptonote::Currency& m_currency) :
      tx_memory_pool(m_currency, validator, timeProvider) {}
  };

  class TxTestBase {
  public:
    TxTestBase(size_t ringSize) :
      m_currency(cryptonote::CurrencyBuilder().currency()),
      txGenerator(m_currency, ringSize),
      pool(m_currency, validator, m_time) 
    {
      txGenerator.createSources();
    }

    void construct(uint64_t fee, size_t outputs, Transaction& tx) {
      txGenerator.construct(txGenerator.m_source_amount, fee, outputs, tx);
    }

    cryptonote::Currency m_currency;
    CryptoNote::RealTimeProvider m_time;
    TestTransactionGenerator txGenerator;
    TransactionValidator validator;
    tx_memory_pool pool;
  };

  void InitBlock(Block& bl, uint8_t majorVersion = BLOCK_MAJOR_VERSION_1) {
    bl.majorVersion = majorVersion;
    bl.minorVersion = 0;
    bl.nonce = 0;
    bl.timestamp = time(0);
    bl.prevId = null_hash;
  }

}

TEST(tx_pool, add_one_tx)
{
  TxTestBase test(1);
  Transaction tx;

  test.construct(test.m_currency.minimumFee(), 1, tx);

  tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
  
  ASSERT_TRUE(test.pool.add_tx(tx, tvc, false));
  ASSERT_FALSE(tvc.m_verifivation_failed);
};

TEST(tx_pool, take_tx)
{
  TxTestBase test(1);
  Transaction tx;

  test.construct(test.m_currency.minimumFee(), 1, tx);

  auto txhash = get_transaction_hash(tx);

  tx_verification_context tvc = boost::value_initialized<tx_verification_context>();

  ASSERT_TRUE(test.pool.add_tx(tx, tvc, false));
  ASSERT_FALSE(tvc.m_verifivation_failed);

  Transaction txOut;
  size_t blobSize;
  uint64_t fee = 0;

  ASSERT_TRUE(test.pool.take_tx(txhash, txOut, blobSize, fee));
  ASSERT_EQ(fee, test.m_currency.minimumFee());
  ASSERT_EQ(tx, txOut);
};


TEST(tx_pool, double_spend_tx)
{
  TxTestBase test(1);
  Transaction tx, tx_double;

  test.construct(test.m_currency.minimumFee(), 1, tx);

  tx_verification_context tvc = boost::value_initialized<tx_verification_context>();

  ASSERT_TRUE(test.pool.add_tx(tx, tvc, false));
  ASSERT_FALSE(tvc.m_verifivation_failed);

  test.txGenerator.rv_acc.generate(); // generate new receiver address
  test.construct(test.m_currency.minimumFee(), 1, tx_double);

  ASSERT_FALSE(test.pool.add_tx(tx_double, tvc, false));
  ASSERT_TRUE(tvc.m_verifivation_failed);
}


TEST(tx_pool, fillblock_same_fee)
{
  cryptonote::Currency currency = cryptonote::CurrencyBuilder().currency();
  TestPool<TransactionValidator, RealTimeProvider> pool(currency);
  uint64_t fee = currency.minimumFee();

  std::unordered_map<crypto::hash, std::unique_ptr<Transaction>> transactions;

  // generate transactions
  for (int i = 1; i <= 50; ++i) {
    TestTransactionGenerator txGenerator(currency, 1);
    txGenerator.createSources();
    
    std::unique_ptr<Transaction> txptr(new Transaction);
    Transaction& tx = *txptr;

    txGenerator.construct(txGenerator.m_source_amount, fee, i, tx);

    tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
    ASSERT_TRUE(pool.add_tx(tx, tvc, false));
    ASSERT_TRUE(tvc.m_added_to_pool);

    transactions[get_transaction_hash(tx)] = std::move(txptr);
  }

  Block bl;

  InitBlock(bl);

  size_t totalSize = 0;
  uint64_t txFee = 0;
  uint64_t median = 5000;

  ASSERT_TRUE(pool.fill_block_template(bl, median, textMaxCumulativeSize, 0, totalSize, txFee));
  ASSERT_TRUE(totalSize*100 < median*125);

  // now, check that the block is opimally filled
  // if fee is fixed, transactions with smaller number of outputs should be included

  size_t maxOuts = 0;

  for (auto& th : bl.txHashes) {
    auto iter = transactions.find(th);
    ASSERT_TRUE(iter != transactions.end());

    size_t txouts = iter->second->vout.size();

    if (txouts > maxOuts)
      maxOuts = txouts;
  }

  ASSERT_TRUE(maxOuts <= bl.txHashes.size());
}


TEST(tx_pool, fillblock_same_size)
{
  cryptonote::Currency currency = cryptonote::CurrencyBuilder().currency();
  TestPool<TransactionValidator, RealTimeProvider> pool(currency);

  const uint64_t fee = currency.minimumFee();
  const size_t totalTransactions = 50;

  std::unordered_map<crypto::hash, std::unique_ptr<Transaction>> transactions;


  // generate transactions
  for (int i = 0; i <= totalTransactions; ++i) {

    TestTransactionGenerator txGenerator(currency, 1);
    txGenerator.createSources();

    std::unique_ptr<Transaction> txptr(new Transaction);
    Transaction& tx = *txptr;

    // interleave fee and fee*2
    txGenerator.construct(txGenerator.m_source_amount, fee + (fee * (i&1)), 1, tx);

    tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
    ASSERT_TRUE(pool.add_tx(tx, tvc, false));
    ASSERT_TRUE(tvc.m_added_to_pool);

    transactions[get_transaction_hash(tx)] = std::move(txptr);
  }


  Block bl;

  InitBlock(bl);

  size_t totalSize = 0;
  uint64_t txFee = 0;
  uint64_t median = 5000;

  ASSERT_TRUE(pool.fill_block_template(bl, median, textMaxCumulativeSize, 0, totalSize, txFee));
  ASSERT_TRUE(totalSize * 100 < median * 125);

  // check that fill_block_template prefers transactions with double fee

  size_t doubleFee = 0;

  for (auto& th : bl.txHashes) {

    auto iter = transactions.find(th);
    ASSERT_TRUE(iter != transactions.end());

    if (get_tx_fee(*iter->second) > fee)
      ++doubleFee;
  }

  ASSERT_TRUE(doubleFee == std::min(bl.txHashes.size(), totalTransactions / 2));

}


TEST(tx_pool, cleanup_stale_tx)
{
  cryptonote::Currency currency = cryptonote::CurrencyBuilder().currency();
  TestPool<TransactionValidator, FakeTimeProvider> pool(currency);
  const uint64_t fee = currency.minimumFee();

  time_t startTime = pool.timeProvider.now();

  for (int i = 0; i < 3; ++i) {
    Transaction tx;
    GenerateTransaction(currency, tx, fee, 1);

    tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
    ASSERT_TRUE(pool.add_tx(tx, tvc, false)); // main chain
    ASSERT_TRUE(tvc.m_added_to_pool);

    pool.timeProvider.timeNow += 60 * 60 * 2; // add 2 hours
  }

  for (int i = 0; i < 5; ++i) {
    Transaction tx;
    GenerateTransaction(currency, tx, fee, 1);

    tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
    ASSERT_TRUE(pool.add_tx(tx, tvc, true)); // alternative chain
    ASSERT_TRUE(tvc.m_added_to_pool);

    pool.timeProvider.timeNow += 60 * 60 * 2; // add 2 hours
  }


  ASSERT_EQ(8, pool.get_transactions_count());

  pool.timeProvider.timeNow = startTime + currency.mempoolTxLiveTime() + 3*60*60; 
  pool.on_idle(); // 2 transactions should be removed

  ASSERT_EQ(6, pool.get_transactions_count());

  pool.timeProvider.timeNow = startTime + currency.mempoolTxFromAltBlockLiveTime() + (3*2+3) * 60 * 60;
  pool.on_idle(); // all transactions from main chain and 2 transactions from altchain should be removed

  ASSERT_EQ(3, pool.get_transactions_count());
}

TEST(tx_pool, add_tx_after_cleanup)
{
  cryptonote::Currency currency = cryptonote::CurrencyBuilder().currency();
  TestPool<TransactionValidator, FakeTimeProvider> pool(currency);
  const uint64_t fee = currency.minimumFee();

  time_t startTime = pool.timeProvider.now();

  Transaction tx;
  GenerateTransaction(currency, tx, fee, 1);

  tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
  ASSERT_TRUE(pool.add_tx(tx, tvc, false)); // main chain
  ASSERT_TRUE(tvc.m_added_to_pool);

  pool.timeProvider.timeNow = startTime + currency.mempoolTxLiveTime() + 1;
  pool.on_idle();

  ASSERT_EQ(0, pool.get_transactions_count());

  // add again
  ASSERT_TRUE(pool.add_tx(tx, tvc, false)); // main chain
  ASSERT_TRUE(tvc.m_added_to_pool);

  ASSERT_EQ(1, pool.get_transactions_count());

}
