// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2017 XDN-project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"

#include <cassert>

#include <CryptoNoteCore/CryptoNoteFormatUtils.h>
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/TransactionExtra.h"
#include "Logging/ConsoleLogger.h"
#include <WalletLegacy/WalletUserTransactionsCache.h>

using namespace CryptoNote;

class WalletUserTransactionsCacheTest : public testing::Test {
public:
  WalletUserTransactionsCacheTest() : currency(CurrencyBuilder(m_logger).currency()) {
    createTxExtraWithPaymentId(stringPaymentId, rawExtra);
    if (!getPaymentIdFromTxExtra(rawExtra, paymentId)) {
      assert(false);
    }
  }

  WalletLegacyTransaction buildTransactionInfo() {
    WalletLegacyTransaction info;
    info.state = WalletLegacyTransactionState::Active;
    info.blockHeight = 1;
    info.totalAmount = 1000;
    info.extra.assign(rawExtra.begin(), rawExtra.end());
    info.hash = paymentId;
    return info;
  }

  TransactionInformation buildTransactionInformation() {
    TransactionInformation info;
    info.blockHeight = 1;
    info.extra.assign(rawExtra.begin(), rawExtra.end());
    info.paymentId = paymentId;
    info.transactionHash = paymentId;

    return info;
  }

  void updateTransaction(const CryptoNote::TransactionInformation& info, int64_t balance) {
    std::vector<TransactionOutputInformation> newDeposits;
    std::vector<TransactionOutputInformation> spentDeposits;
    cache.onTransactionUpdated(info, balance, newDeposits, spentDeposits, currency);
  }

  Logging::ConsoleLogger m_logger;
  Currency currency;
  std::string stringPaymentId = "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef";
  WalletUserTransactionsCache cache;
  PaymentId paymentId;
  std::vector<uint8_t> rawExtra;
  Crypto::Hash hash;
  TransactionId id = 0;
};

TEST_F(WalletUserTransactionsCacheTest, TransactionIsAddedToIndexWhenItIsConfirmed) {
  updateTransaction(buildTransactionInformation(), 1000);
  ASSERT_EQ(1, cache.getTransactionsByPaymentIds({paymentId})[0].transactions.size());
  ASSERT_EQ(paymentId, cache.getTransactionsByPaymentIds({paymentId})[0].transactions[0].hash);
}

TEST_F(WalletUserTransactionsCacheTest, TransactionWithInvalidHeightIsNotAdded) {
  auto tx = buildTransactionInformation();
  tx.blockHeight = WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT;
  updateTransaction(tx, 1000);
  ASSERT_EQ(0, cache.getTransactionsByPaymentIds({paymentId})[0].transactions.size());
}

TEST_F(WalletUserTransactionsCacheTest, TransactionWithEmptyExtraIsNotAdded) {
  auto tx = buildTransactionInformation();
  tx.extra.clear();
  updateTransaction(tx, 1000);
  ASSERT_EQ(0, cache.getTransactionsByPaymentIds({paymentId})[0].transactions.size());
}

TEST_F(WalletUserTransactionsCacheTest, TransactionWithInvalidAmountIsNotAdded) {
  updateTransaction(buildTransactionInformation(), 0);
  ASSERT_EQ(0, cache.getTransactionsByPaymentIds({paymentId})[0].transactions.size());
}

TEST_F(WalletUserTransactionsCacheTest, TransactionIsRemovedFromIndexWhenItIsRemovedFromCache) {
  updateTransaction(buildTransactionInformation(), 1000);
  ASSERT_EQ(1, cache.getTransactionsByPaymentIds({paymentId})[0].transactions.size());
  cache.onTransactionDeleted(cache.getTransaction(id).hash);
  ASSERT_EQ(0, cache.getTransactionsByPaymentIds({paymentId})[0].transactions.size());
}
