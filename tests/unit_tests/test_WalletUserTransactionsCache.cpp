// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"

#include <cassert>
#include <cryptonote_core/cryptonote_format_utils.h>
#include <wallet/WalletUserTransactionsCache.h>

using namespace CryptoNote;

class WalletUserTransactionsCacheTest : public testing::Test {
public:
  WalletUserTransactionsCacheTest() {
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

  std::string stringPaymentId = "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef";
  WalletUserTransactionsCache cache;
  PaymentId paymentId;
  std::vector<uint8_t> rawExtra;
  crypto::hash hash;
  TransactionId id = 0;
};

TEST_F(WalletUserTransactionsCacheTest, TransactionIsAddedToIndexWhenItIsConfirmed) {
  cache.onTransactionUpdated(buildTransactionInformation(), 1000);
  ASSERT_EQ(1, cache.getTransactionsByPaymentIds({paymentId})[0].transactions.size());
  ASSERT_EQ(paymentId, cache.getTransactionsByPaymentIds({paymentId})[0].transactions[0].hash);
}

TEST_F(WalletUserTransactionsCacheTest, TransactionWithInvalidHeightIsNotAdded) {
  auto tx = buildTransactionInformation();
  tx.blockHeight = UNCONFIRMED_TRANSACTION_HEIGHT;
  cache.onTransactionUpdated(tx, 1000);
  ASSERT_EQ(0, cache.getTransactionsByPaymentIds({paymentId})[0].transactions.size());
}

TEST_F(WalletUserTransactionsCacheTest, TransactionWithEmptyExtraIsNotAdded) {
  auto tx = buildTransactionInformation();
  tx.extra.clear();
  cache.onTransactionUpdated(tx, 1000);
  ASSERT_EQ(0, cache.getTransactionsByPaymentIds({paymentId})[0].transactions.size());
}

TEST_F(WalletUserTransactionsCacheTest, TransactionWithInvalidAmountIsNotAdded) {
  cache.onTransactionUpdated(buildTransactionInformation(), 0);
  ASSERT_EQ(0, cache.getTransactionsByPaymentIds({paymentId})[0].transactions.size());
}

TEST_F(WalletUserTransactionsCacheTest, TransactionIsRemovedFromIndexWhenItIsRemovedFromCache) {
  cache.onTransactionUpdated(buildTransactionInformation(), 1000);
  ASSERT_EQ(1, cache.getTransactionsByPaymentIds({paymentId})[0].transactions.size());
  cache.onTransactionDeleted(cache.getTransaction(id).hash);
  ASSERT_EQ(0, cache.getTransactionsByPaymentIds({paymentId})[0].transactions.size());
}
