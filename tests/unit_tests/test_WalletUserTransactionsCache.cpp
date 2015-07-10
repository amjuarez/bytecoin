// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"

#include <cassert>

#include <cryptonote_core/cryptonote_format_utils.h>
#include "cryptonote_core/Currency.h"
#include <wallet/WalletUserTransactionsCache.h>

using namespace CryptoNote;
using namespace cryptonote;

class WalletUserTransactionsCacheTest : public testing::Test {
public:
  WalletUserTransactionsCacheTest() : currency(CurrencyBuilder().currency()) {
    cryptonote::createTxExtraWithPaymentId(stringPaymentId, rawExtra);
    crypto::hash hash;
    if (!cryptonote::getPaymentIdFromTxExtra(rawExtra, hash)) {
      assert(false);
    }
    std::copy_n(reinterpret_cast<unsigned char*>(&hash), sizeof(hash), paymentId.begin());
  }

  TransactionInfo buildTransactionInfo() {
    TransactionInfo info;
    info.state = TransactionState::Active;
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

  Currency currency;
  std::string stringPaymentId = "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef";
  WalletUserTransactionsCache cache;
  PaymentId paymentId;
  std::vector<uint8_t> rawExtra;
  crypto::hash hash;
  TransactionId id = 0;
};

TEST_F(WalletUserTransactionsCacheTest, TransactionIsAddedToIndexWhenItIsConfirmed) {
  updateTransaction(buildTransactionInformation(), 1000);
  ASSERT_EQ(1, cache.getTransactionsByPaymentIds({paymentId})[0].transactions.size());
  ASSERT_EQ(paymentId, cache.getTransactionsByPaymentIds({paymentId})[0].transactions[0].hash);
}

TEST_F(WalletUserTransactionsCacheTest, TransactionWithInvalidHeightIsNotAdded) {
  auto tx = buildTransactionInformation();
  tx.blockHeight = UNCONFIRMED_TRANSACTION_HEIGHT;
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
