// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"

#include <cryptonote_core/DepositIndex.h>

using namespace CryptoNote;

class DepositIndexTest : public ::testing::Test {
public:
  const std::size_t DEFAULT_HEIGHT = 10;
  DepositIndexTest() : index(DEFAULT_HEIGHT) {
  }
  DepositIndex index;
};

TEST_F(DepositIndexTest, EmptyAfterCreate) {
  ASSERT_EQ(0, index.fullDepositAmount());
}

TEST_F(DepositIndexTest, AddBlockUpdatesGlobalAmount) {
  index.pushBlock(10, 1);
  ASSERT_EQ(10, index.fullDepositAmount());
}

TEST_F(DepositIndexTest, RemoveReducesGlobalAmount) {
  index.pushBlock(12, 1);
  index.popBlock();
  ASSERT_EQ(0, index.fullDepositAmount());
}

TEST_F(DepositIndexTest, AddEmptyBlockIncrementsHeight) {
  index.pushBlock(0, 0);
  ASSERT_EQ(1, index.lastHeight());
  index.pushBlock(0, 0);
  ASSERT_EQ(2, index.lastHeight());
}

TEST_F(DepositIndexTest, MultipleRemoves) {
  index.pushBlock(10, 1);
  index.pushBlock(0, 0);
  index.pushBlock(11, 1);
  index.pushBlock(0, 0);
  index.pushBlock(12, 1);
  ASSERT_EQ(5, index.popBlocks(0));
  ASSERT_EQ(0, index.fullDepositAmount());
}

TEST_F(DepositIndexTest, MultipleRemovesDecrementHeight) {
  index.pushBlock(10, 1);
  index.pushBlock(11, 1);
  index.pushBlock(0, 0);
  index.pushBlock(12, 1);
  ASSERT_EQ(2, index.popBlocks(3));
  ASSERT_EQ(2, index.lastHeight());
}

TEST_F(DepositIndexTest, PopBlockReducesFullAmount) {
  index.pushBlock(10, 1);
  index.pushBlock(12, 1);
  index.popBlock();
  ASSERT_EQ(10, index.fullDepositAmount());
}

TEST_F(DepositIndexTest, RemoveDoesntClearGlobalAmount) {
  index.pushBlock(9, 1);
  index.pushBlock(12, 1);
  index.popBlock();
  ASSERT_EQ(9, index.fullDepositAmount());
}

TEST_F(DepositIndexTest, AddEmptyBlockDoesntChangeAmount) {
  index.pushBlock(9, 1);
  index.pushBlock(0, 0);
  ASSERT_EQ(9, index.fullDepositAmount());
}

TEST_F(DepositIndexTest, AddEmptyBlockDoesntChangeInterest) {
  index.pushBlock(9, 1);
  index.pushBlock(0, 0);
  ASSERT_EQ(1, index.fullInterestAmount());
}

TEST_F(DepositIndexTest, RemoveDecrementsHeight) {
  index.pushBlock(9, 1);
  index.pushBlock(12, 1);
  index.popBlock();
  ASSERT_EQ(1, index.lastHeight());
}

TEST_F(DepositIndexTest, GlobalAmountIsSumOfBlockDeposits) {
  index.pushBlock(9, 1);
  index.pushBlock(12, 1);
  ASSERT_EQ(9 + 12, index.fullDepositAmount());
}

TEST_F(DepositIndexTest, DepositAmountAtHeightInTheMiddle) {
  index.pushBlock(9, 1);
  index.pushBlock(12, 1);
  index.pushBlock(14, 1);
  ASSERT_EQ(9 + 12, index.depositAmountAtHeight(2));
}

TEST_F(DepositIndexTest, MaxAmountIsReturnedForHeightLargerThanLastBlock) {
  index.pushBlock(9, 1);
  index.pushBlock(12, 1);
  index.pushBlock(14, 1);
  ASSERT_EQ(index.depositAmountAtHeight(20), index.fullDepositAmount());
}

TEST_F(DepositIndexTest, DepositAmountAtHeightInTheMiddleLooksForLowerBound) {
  index.pushBlock(9, 1);
  index.pushBlock(12, 1);
  index.pushBlock(14, 1);
  index.pushBlock(7, 1);
  ASSERT_EQ(9 + 12 + 14, index.depositAmountAtHeight(3));
}

TEST_F(DepositIndexTest, DepositAmountAtHeightInTheMiddleIgnoresEmptyBlocks) {
  index.pushBlock(9, 1);
  index.pushBlock(0, 0);
  index.pushBlock(12, 1);
  index.pushBlock(0, 0);
  index.pushBlock(14, 1);
  index.pushBlock(0, 0);
  index.pushBlock(7, 1);
  ASSERT_EQ(9 + 12, index.depositAmountAtHeight(3));
}

TEST_F(DepositIndexTest, AmountAtZeroHeightIsZero) {
  index.pushBlock(9, 1);
  index.pushBlock(12, 1);
  index.pushBlock(14, 1);
  ASSERT_EQ(0, index.depositAmountAtHeight(0));
}

TEST_F(DepositIndexTest, MultiPopZeroChangesNothing) {
  ASSERT_EQ(0, index.popBlocks(0));
  ASSERT_EQ(0, index.depositAmountAtHeight(0));
}

TEST_F(DepositIndexTest, DepositAmountAtNonExistingHeight) {
  ASSERT_EQ(0, index.depositAmountAtHeight(4));
}

TEST_F(DepositIndexTest, MultiPopZeroClearsIndex) {
  index.pushBlock(9, 1);
  index.pushBlock(12, 1);
  index.pushBlock(14, 1);
  ASSERT_EQ(3, index.popBlocks(0));
  ASSERT_EQ(0, index.depositAmountAtHeight(0));
}

TEST_F(DepositIndexTest, GetInterestOnHeight) {
  index.pushBlock(9, 1);
  index.pushBlock(12, 1);
  index.pushBlock(14, 1);
  ASSERT_EQ(3, index.depositInterestAtHeight(14));
}

TEST_F(DepositIndexTest, CanSubmitNegativeDeposit) {
  index.pushBlock(20, 1);
  index.pushBlock(-14, 1);
}

TEST_F(DepositIndexTest, DepositAmountCanBeReduced) {
  index.pushBlock(9, 1);
  index.pushBlock(12, 1);
  index.pushBlock(-14, 1);
  ASSERT_EQ(9 + 12 - 14, index.fullDepositAmount());
}

TEST_F(DepositIndexTest, PopBlocksZeroReturnsZero) {
  ASSERT_EQ(0, index.popBlocks(0));
}

TEST_F(DepositIndexTest, PopBlocksZeroRemovesEmptyBlocks) {
  index.pushBlock(1, 1);
  index.pushBlock(0, 0);
  ASSERT_EQ(1, index.popBlocks(2));
  ASSERT_EQ(1, index.lastHeight());
  ASSERT_EQ(1, index.fullDepositAmount());
  ASSERT_EQ(1, index.fullInterestAmount());
}
