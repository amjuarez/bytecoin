// Copyright (c) 2011-2016 The Cryptonote developers
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
