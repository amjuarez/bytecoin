// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <cryptonote_core/Currency.h>

#include <gtest/gtest.h>

#include <cryptonote_core/TransactionApi.h>

using namespace cryptonote;
using namespace CryptoNote;

class CurrencyTest : public testing::Test {
public:
  const uint64_t fixed_amount = 1000;
  const uint32_t fixed_term = 400;
  CurrencyTest()
      : defaultCurrency(builder.currency()), fixedCurrency(builder.depositMaxTotalRate(10)
                                                               .depositMinTotalRateFactor(10)
                                                               .depositMinTerm(1)
                                                               .depositMaxTerm(401)
                                                               .currency()) {
  }
  void setupTransactionInputs(int inputs = 1) {
    while (inputs --> 0) {
      transaction.vin.push_back(TransactionInputMultisignature{fixed_amount, 3, 4, fixed_term});
    }
  }
  void setupTransactionOutputs(int outputs = 1) {
    while (outputs --> 0) {
      transaction.vout.push_back(TransactionOutput{fixed_amount, TransactionOutputMultisignature{{}, 1, fixed_term}});
    }
  }
  CurrencyBuilder builder;
  Transaction transaction;
  Currency defaultCurrency;
  Currency fixedCurrency;
  uint64_t fee;
};

TEST_F(CurrencyTest, calculateInterestZero) {
  auto currency = builder.depositMaxTotalRate(1).depositMinTotalRateFactor(0).depositMinTerm(0).depositMaxTerm(1).currency();
  ASSERT_TRUE(currency.calculateInterest(0, 1) == 0);
}

TEST_F(CurrencyTest, calculateInterestReal) {
  ASSERT_EQ(fixedCurrency.calculateInterest(fixed_amount, fixed_term), 99);
}

TEST_F(CurrencyTest, calculateInterestNoOverflow) {
  auto currency = builder.depositMaxTotalRate(100).depositMinTotalRateFactor(0).depositMaxTerm(100000).currency();
  ASSERT_EQ(currency.calculateInterest(0xffffffffffff, 100000), 0xffffffffffff);
}

TEST_F(CurrencyTest, calculateTotalTransactionInterestEmpty) {
  auto currency = builder.depositMaxTotalRate(0).depositMinTotalRateFactor(0).depositMaxTerm(1).currency();
  ASSERT_EQ(currency.calculateTotalTransactionInterest(transaction), 0);
}

TEST_F(CurrencyTest, calculateTotalTransactionInterestOneTransaction) {
  setupTransactionInputs();
  ASSERT_EQ(transaction.vin.size(), 1);
  ASSERT_EQ(fixedCurrency.calculateTotalTransactionInterest(transaction), 99);
}

TEST_F(CurrencyTest, calculateTotalTransactionInterestThreeTransactions) {
  setupTransactionInputs(3);
  ASSERT_EQ(transaction.vin.size(), 3);
  ASSERT_EQ(fixedCurrency.calculateTotalTransactionInterest(transaction), 99 * 3);
}

TEST_F(CurrencyTest, calculateTotalTransactionInterestNonDepositInput) {
  transaction.vin.push_back(TransactionInputMultisignature{1, 2, 4, 0});
  ASSERT_EQ(transaction.vin.size(), 1);
  ASSERT_EQ(fixedCurrency.calculateTotalTransactionInterest(transaction), 0);
}

TEST_F(CurrencyTest, calculateTotalTransactionInterestMixedInput) {
  setupTransactionInputs(10);
  transaction.vin.push_back(TransactionInputMultisignature{1, 2, 4, 0});
  transaction.vin.push_back(TransactionInputMultisignature{1, 2, 4, 0});
  ASSERT_EQ(transaction.vin.size(), 12);
  ASSERT_EQ(fixedCurrency.calculateTotalTransactionInterest(transaction), 99 * 10);
}

TEST_F(CurrencyTest, getTransactionInputAmountInputToKey) {
  ASSERT_EQ(defaultCurrency.getTransactionInputAmount(TransactionInputToKey{10, {}, {}}), 10);
}

TEST_F(CurrencyTest, getTransactionInputAmountMultisignature) {
  ASSERT_EQ(defaultCurrency.getTransactionInputAmount(TransactionInputMultisignature{10, 1, 2, 0}), 10);
}

TEST_F(CurrencyTest, getTransactionInputAmountDeposit) {
  ASSERT_EQ(fixedCurrency.getTransactionInputAmount(TransactionInputMultisignature{fixed_amount, 1, 2, fixed_term}), fixed_amount + 99);
}

TEST_F(CurrencyTest, getTransactionAllInputsAmountZero) {
  ASSERT_EQ(fixedCurrency.getTransactionAllInputsAmount(transaction), 0);
}

TEST_F(CurrencyTest, getTransactionAllInputsAmountThreeDeposits) {
  setupTransactionInputs(3);
  ASSERT_EQ(fixedCurrency.getTransactionAllInputsAmount(transaction), (fixed_amount + 99) * 3);
}

TEST_F(CurrencyTest, getTransactionAllInputsAmountMixedInput) {
  setupTransactionInputs(3);
  transaction.vin.push_back(TransactionInputMultisignature{10, 2, 3, 0});
  transaction.vin.push_back(TransactionInputMultisignature{11, 2, 3, 0});
  ASSERT_EQ(fixedCurrency.getTransactionAllInputsAmount(transaction), (fixed_amount + 99) * 3 + 10 + 11);
}

TEST_F(CurrencyTest, getTransactionFeeZero) {
  ASSERT_EQ(fixedCurrency.getTransactionFee(transaction), 0);
}

TEST_F(CurrencyTest, getTransactionFeeOnlyOutputs) {
  setupTransactionInputs(0);
  setupTransactionOutputs(2);
  ASSERT_EQ(fixedCurrency.getTransactionFee(transaction), 0);
}

TEST_F(CurrencyTest, getTransactionFeeRefOnlyOutputs) {
  setupTransactionInputs(0);
  setupTransactionOutputs(2);
  ASSERT_FALSE(fixedCurrency.getTransactionFee(transaction, fee));
}

TEST_F(CurrencyTest, getTransactionFeeEqualInputsOutputs) {
  setupTransactionInputs(2);
  setupTransactionOutputs(2);
  ASSERT_EQ(fixedCurrency.getTransactionFee(transaction), fixedCurrency.calculateInterest(fixed_amount, fixed_term) * 2);
}

TEST_F(CurrencyTest, getTransactionFeeRefEqualInputsOutputs) {
  setupTransactionInputs(2);
  setupTransactionOutputs(2);
  ASSERT_TRUE(fixedCurrency.getTransactionFee(transaction, fee));
  ASSERT_EQ(fee, fixedCurrency.calculateInterest(fixed_amount, fixed_term) * 2);
}

TEST_F(CurrencyTest, getTransactionFeeOnlyInputs) {
  setupTransactionInputs(2);
  setupTransactionOutputs(0);
  ASSERT_EQ(fixedCurrency.getTransactionFee(transaction), (fixedCurrency.calculateInterest(fixed_amount, fixed_term) + fixed_amount) * 2);
}

TEST_F(CurrencyTest, getTransactionFeeRefOnlyInputs) {
  setupTransactionInputs(2);
  setupTransactionOutputs(0);
  ASSERT_TRUE(fixedCurrency.getTransactionFee(transaction, fee));
  ASSERT_EQ(fee, (fixedCurrency.calculateInterest(fixed_amount, fixed_term) + fixed_amount) * 2);
}

