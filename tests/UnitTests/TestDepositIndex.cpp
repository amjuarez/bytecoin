// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2017 XDN-project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "gtest/gtest.h"

#include <CryptoNoteCore/DepositIndex.h>

using namespace CryptoNote;

class DepositIndexTest : public ::testing::Test {
public:
  const std::size_t DEFAULT_HEIGHT = 10;
  DepositIndexTest() : index(static_cast<DepositIndex::DepositHeight>(DEFAULT_HEIGHT)) {
  }
  DepositIndex index;
};

TEST_F(DepositIndexTest, EmptyAfterCreate) {
  ASSERT_EQ(0, index.fullDepositAmount());
  ASSERT_EQ(0, index.fullInterestAmount());
}

TEST_F(DepositIndexTest, AddBlockUpdatesGlobalAmount) {
  index.pushBlock(10, 1);
  ASSERT_EQ(10, index.fullDepositAmount());
}

TEST_F(DepositIndexTest, AddBlockUpdatesFullInterest) {
  index.pushBlock(10, 1);
  ASSERT_EQ(1, index.fullInterestAmount());
}

TEST_F(DepositIndexTest, GlobalAmountIsSumOfBlockDeposits) {
  index.pushBlock(9, 1);
  index.pushBlock(12, 1);
  ASSERT_EQ(9 + 12, index.fullDepositAmount());
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

TEST_F(DepositIndexTest, FirstBlockPushUpdatesDepositAmountAtHeight0) {
  index.pushBlock(9, 1);
  ASSERT_EQ(9, index.depositAmountAtHeight(0));
}

TEST_F(DepositIndexTest, FirstBlockPushUpdatesDepositInterestAtHeight0) {
  index.pushBlock(9, 1);
  ASSERT_EQ(1, index.depositInterestAtHeight(0));
}

TEST_F(DepositIndexTest, FullDepositAmountEqualsDepositAmountAtLastHeight) {
  index.pushBlock(9, 1);
  index.pushBlock(12, 2);
  ASSERT_EQ(index.fullDepositAmount(), index.depositAmountAtHeight(index.size() - 1));
}

TEST_F(DepositIndexTest, FullInterestAmountEqualsDepositInterestAtLastHeight) {
  index.pushBlock(9, 1);
  index.pushBlock(12, 2);
  ASSERT_EQ(index.fullInterestAmount(), index.depositInterestAtHeight(index.size() - 1));
}

TEST_F(DepositIndexTest, FullDepositAmountEqualsDepositAmountAtHeightGreaterThanLastHeight) {
  index.pushBlock(9, 1);
  index.pushBlock(12, 2);
  ASSERT_EQ(index.fullDepositAmount(), index.depositAmountAtHeight(index.size()));
}

TEST_F(DepositIndexTest, FullInterestAmountEqualsInterestAmountAtHeightGreaterThanLastHeight) {
  index.pushBlock(9, 1);
  index.pushBlock(12, 2);
  ASSERT_EQ(index.fullInterestAmount(), index.depositInterestAtHeight(index.size()));
}

TEST_F(DepositIndexTest, RemoveReducesGlobalAmount) {
  index.pushBlock(12, 1);
  index.popBlock();
  ASSERT_EQ(0, index.fullDepositAmount());
}

TEST_F(DepositIndexTest, AddEmptyBlockIncrementsSize) {
  index.pushBlock(0, 0);
  ASSERT_EQ(1, index.size());
  index.pushBlock(0, 0);
  ASSERT_EQ(2, index.size());
}

TEST_F(DepositIndexTest, PopEmptyBlockDecrementsSize) {
  index.pushBlock(0, 0);
  index.popBlock();
  ASSERT_EQ(0, index.size());
}

TEST_F(DepositIndexTest, AddNonEmptyBlockIncrementsSize) {
  index.pushBlock(9, 1);
  ASSERT_EQ(1, index.size());
  index.pushBlock(12, 1);
  ASSERT_EQ(2, index.size());
}

TEST_F(DepositIndexTest, PopNonEmptyBlockDecrementsSize) {
  index.pushBlock(9, 1);
  index.popBlock();
  ASSERT_EQ(0, index.size());
}

TEST_F(DepositIndexTest, PopLastEmptyBlockDoesNotChangeFullDepositAmount) {
  index.pushBlock(9, 1);
  index.pushBlock(0, 0);
  index.popBlock();
  ASSERT_EQ(9, index.fullDepositAmount());
}

TEST_F(DepositIndexTest, PopLastEmptyBlockDoesNotChangeFullInterestAmount) {
  index.pushBlock(9, 1);
  index.pushBlock(0, 0);
  index.popBlock();
  ASSERT_EQ(1, index.fullInterestAmount());
}

TEST_F(DepositIndexTest, MultipleRemovals) {
  index.pushBlock(10, 1);
  index.pushBlock(0, 0);
  index.pushBlock(11, 1);
  index.pushBlock(0, 0);
  index.pushBlock(12, 1);
  ASSERT_EQ(5, index.popBlocks(0));
  ASSERT_EQ(0, index.fullDepositAmount());
}

TEST_F(DepositIndexTest, MultipleRemovalsDecrementSize) {
  index.pushBlock(10, 1);
  index.pushBlock(11, 1);
  index.pushBlock(0, 0);
  index.pushBlock(12, 1);
  ASSERT_EQ(1, index.popBlocks(3));
  ASSERT_EQ(4 - 1, index.size());
}

TEST_F(DepositIndexTest, PopBlockReducesFullAmount) {
  index.pushBlock(10, 1);
  index.pushBlock(12, 1);
  index.popBlock();
  ASSERT_EQ(10, index.fullDepositAmount());
}

TEST_F(DepositIndexTest, PopBlockDecrementsSize) {
  index.pushBlock(9, 1);
  index.pushBlock(12, 1);

  auto size = index.size();
  index.popBlock();
  ASSERT_EQ(size - 1, index.size());
}

TEST_F(DepositIndexTest, DepositAmountAtAnyHeightIsZeroAfterCreation) {
  ASSERT_EQ(0, index.depositAmountAtHeight(10));
}

TEST_F(DepositIndexTest, DepositInterestAtAnyHeightIsZeroAfterCreation) {
  ASSERT_EQ(0, index.depositInterestAtHeight(10));
}

TEST_F(DepositIndexTest, DepositAmountIsZeroAtAnyHeightBeforeFirstDeposit) {
  index.pushBlock(0, 0);
  index.pushBlock(9, 1);
  ASSERT_EQ(0, index.depositAmountAtHeight(0));
}

TEST_F(DepositIndexTest, DepositInterestIsZeroAtAnyHeightBeforeFirstDeposit) {
  index.pushBlock(0, 0);
  index.pushBlock(9, 1);
  ASSERT_EQ(0, index.depositInterestAtHeight(0));
}

TEST_F(DepositIndexTest, DepositAmountAtHeightInTheMiddle) {
  index.pushBlock(9, 1);
  index.pushBlock(12, 1);
  index.pushBlock(14, 1);
  ASSERT_EQ(9 + 12, index.depositAmountAtHeight(1));
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
  ASSERT_EQ(9 + 12 + 14, index.depositAmountAtHeight(2));
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

TEST_F(DepositIndexTest, PopBlocksRemovesEmptyBlocks) {
  index.pushBlock(1, 1);
  index.pushBlock(0, 0);
  index.pushBlock(0, 0);
  ASSERT_EQ(2, index.popBlocks(1));
  ASSERT_EQ(1, index.size());
  ASSERT_EQ(1, index.fullDepositAmount());
  ASSERT_EQ(1, index.fullInterestAmount());
}
