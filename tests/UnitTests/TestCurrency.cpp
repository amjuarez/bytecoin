// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2017 XDN-project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"

#include "crypto/crypto.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/TransactionApi.h"
#include "Logging/ConsoleLogger.h"

#include "TransactionApiHelpers.h"

using namespace CryptoNote;

namespace {

class CurrencyTest : public testing::Test {
public:
  const uint64_t fixed_amount = 1000;
  const uint32_t fixed_term = 400;
  CurrencyTest() :
        builder(m_logger),
        defaultCurrency(builder.currency()), fixedCurrency(builder.depositMaxTotalRate(10)
                                                               .depositMinTotalRateFactor(10)
                                                               .depositMinTerm(1)
                                                               .depositMaxTerm(401)
                                                               .currency()) {
  }
  void setupTransactionInputs(int inputs = 1) {
    while (inputs --> 0) {
      transaction.inputs.push_back(MultisignatureInput{fixed_amount, 3, 4, fixed_term});
    }
  }
  void setupTransactionOutputs(int outputs = 1) {
    while (outputs --> 0) {
      transaction.outputs.push_back(TransactionOutput{fixed_amount, MultisignatureOutput{{}, 1, fixed_term}});
    }
  }

  Logging::ConsoleLogger m_logger;
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
  ASSERT_EQ(transaction.inputs.size(), 1);
  ASSERT_EQ(fixedCurrency.calculateTotalTransactionInterest(transaction), 99);
}

TEST_F(CurrencyTest, calculateTotalTransactionInterestThreeTransactions) {
  setupTransactionInputs(3);
  ASSERT_EQ(transaction.inputs.size(), 3);
  ASSERT_EQ(fixedCurrency.calculateTotalTransactionInterest(transaction), 99 * 3);
}

TEST_F(CurrencyTest, calculateTotalTransactionInterestNonDepositInput) {
  transaction.inputs.push_back(MultisignatureInput{ 1, 2, 4, 0 });
  ASSERT_EQ(transaction.inputs.size(), 1);
  ASSERT_EQ(fixedCurrency.calculateTotalTransactionInterest(transaction), 0);
}

TEST_F(CurrencyTest, calculateTotalTransactionInterestMixedInput) {
  setupTransactionInputs(10);
  transaction.inputs.push_back(MultisignatureInput{ 1, 2, 4, 0 });
  transaction.inputs.push_back(MultisignatureInput{ 1, 2, 4, 0 });
  ASSERT_EQ(transaction.inputs.size(), 12);
  ASSERT_EQ(fixedCurrency.calculateTotalTransactionInterest(transaction), 99 * 10);
}

TEST_F(CurrencyTest, getTransactionInputAmountInputToKey) {
  ASSERT_EQ(defaultCurrency.getTransactionInputAmount(KeyInput{10, {}, {}}), 10);
}

TEST_F(CurrencyTest, getTransactionInputAmountMultisignature) {
  ASSERT_EQ(defaultCurrency.getTransactionInputAmount(MultisignatureInput{10, 1, 2, 0}), 10);
}

TEST_F(CurrencyTest, getTransactionInputAmountDeposit) {
  ASSERT_EQ(fixedCurrency.getTransactionInputAmount(MultisignatureInput{fixed_amount, 1, 2, fixed_term}), fixed_amount + 99);
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
  transaction.inputs.push_back(MultisignatureInput{ 10, 2, 3, 0 });
  transaction.inputs.push_back(MultisignatureInput{ 11, 2, 3, 0 });
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

const size_t TEST_FUSION_TX_MAX_SIZE = 6000;
const size_t TEST_FUSION_TX_MIN_INPUT_COUNT = 6;
const size_t TEST_FUSION_TX_MIN_IN_OUT_COUNT_RATIO = 3;
const uint64_t TEST_DUST_THRESHOLD = UINT64_C(1000000);
const uint64_t TEST_AMOUNT = 370 * TEST_DUST_THRESHOLD;

class Currency_isFusionTransactionTest : public ::testing::Test {
public:
  Currency_isFusionTransactionTest() :
    m_currency(CurrencyBuilder(m_logger).
      defaultDustThreshold(TEST_DUST_THRESHOLD).
      fusionTxMaxSize(TEST_FUSION_TX_MAX_SIZE).
      fusionTxMinInputCount(TEST_FUSION_TX_MIN_INPUT_COUNT).
      fusionTxMinInOutCountRatio(TEST_FUSION_TX_MIN_IN_OUT_COUNT_RATIO).
      currency()) {
  }

protected:
  Logging::ConsoleLogger m_logger;
  Currency m_currency;
};
}

TEST_F(Currency_isFusionTransactionTest, succeedsOnFusionTransaction) {
  auto tx = FusionTransactionBuilder(m_currency, TEST_AMOUNT).buildTx();
  ASSERT_TRUE(m_currency.isFusionTransaction(tx));
}

TEST_F(Currency_isFusionTransactionTest, succeedsIfFusionTransactionSizeEqMaxSize) {
  FusionTransactionBuilder builder(m_currency, TEST_AMOUNT);
  auto tx = builder.createFusionTransactionBySize(m_currency.fusionTxMaxSize());
  ASSERT_EQ(m_currency.fusionTxMaxSize(), getObjectBinarySize(tx));
  ASSERT_TRUE(m_currency.isFusionTransaction(tx));
}

TEST_F(Currency_isFusionTransactionTest, failsIfFusionTransactionSizeGreaterThanMaxSize) {
  FusionTransactionBuilder builder(m_currency, TEST_AMOUNT);
  auto tx = builder.createFusionTransactionBySize(m_currency.fusionTxMaxSize() + 1);
  ASSERT_EQ(m_currency.fusionTxMaxSize() + 1, getObjectBinarySize(tx));
  ASSERT_FALSE(m_currency.isFusionTransaction(tx));
}

TEST_F(Currency_isFusionTransactionTest, failsIfTransactionInputsCountIsNotEnought) {
  FusionTransactionBuilder builder(m_currency, TEST_AMOUNT);
  builder.setInputCount(m_currency.fusionTxMinInputCount() - 1);
  auto tx = builder.buildTx();
  ASSERT_EQ(m_currency.fusionTxMinInputCount() - 1, tx.inputs.size());
  ASSERT_FALSE(m_currency.isFusionTransaction(tx));
}

TEST_F(Currency_isFusionTransactionTest, failsIfTransactionInputOutputCountRatioIsLessThenNecessary) {
  FusionTransactionBuilder builder(m_currency, 3710 * m_currency.defaultDustThreshold());
  auto tx = builder.buildTx();
  ASSERT_EQ(3, tx.outputs.size());
  ASSERT_GT(tx.outputs.size() * m_currency.fusionTxMinInOutCountRatio(), tx.inputs.size());
  ASSERT_FALSE(m_currency.isFusionTransaction(tx));
}

TEST_F(Currency_isFusionTransactionTest, failsIfTransactionHasNotExponentialOutput) {
  FusionTransactionBuilder builder(m_currency, TEST_AMOUNT);
  builder.setFirstOutput(TEST_AMOUNT);
  auto tx = builder.buildTx();
  ASSERT_EQ(1, tx.outputs.size());
  ASSERT_FALSE(m_currency.isFusionTransaction(tx));
}

TEST_F(Currency_isFusionTransactionTest, failsIfTransactionHasOutputsWithTheSameExponent) {
  FusionTransactionBuilder builder(m_currency, 130 * m_currency.defaultDustThreshold());
  builder.setFirstOutput(70 * m_currency.defaultDustThreshold());
  auto tx = builder.buildTx();
  ASSERT_EQ(2, tx.outputs.size());
  ASSERT_FALSE(m_currency.isFusionTransaction(tx));
}

TEST_F(Currency_isFusionTransactionTest, succeedsIfTransactionHasDustOutput) {
  FusionTransactionBuilder builder(m_currency, 11 * m_currency.defaultDustThreshold());
  auto tx = builder.buildTx();
  ASSERT_EQ(2, tx.outputs.size());
  ASSERT_EQ(m_currency.defaultDustThreshold(), tx.outputs[0].amount);
  ASSERT_TRUE(m_currency.isFusionTransaction(tx));
}

TEST_F(Currency_isFusionTransactionTest, failsIfTransactionFeeIsNotZero) {
  FusionTransactionBuilder builder(m_currency, 370 * m_currency.defaultDustThreshold());
  builder.setFee(70 * m_currency.defaultDustThreshold());
  auto tx = builder.buildTx();
  ASSERT_FALSE(m_currency.isFusionTransaction(tx));
}

TEST_F(Currency_isFusionTransactionTest, succedsIfTransactionHasInputEqualsDustThreshold) {
  FusionTransactionBuilder builder(m_currency, TEST_AMOUNT);
  builder.setFirstInput(m_currency.defaultDustThreshold());
  auto tx = builder.buildTx();
  ASSERT_TRUE(m_currency.isFusionTransaction(tx));
}

TEST_F(Currency_isFusionTransactionTest, failsIfTransactionHasInputLessThanDustThreshold) {
  FusionTransactionBuilder builder(m_currency, TEST_AMOUNT);
  builder.setFirstInput(m_currency.defaultDustThreshold() - 1);
  auto tx = builder.buildTx();
  ASSERT_FALSE(m_currency.isFusionTransaction(tx));
}
