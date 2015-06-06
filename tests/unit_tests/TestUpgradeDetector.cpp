// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <vector>

#include "gtest/gtest.h"

#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/UpgradeDetector.h"

namespace {
  using cryptonote::BLOCK_MAJOR_VERSION_1;
  using cryptonote::BLOCK_MAJOR_VERSION_2;
  using cryptonote::BLOCK_MINOR_VERSION_0;
  using cryptonote::BLOCK_MINOR_VERSION_1;

  struct BlockEx {
    cryptonote::Block bl;
  };

  typedef std::vector<BlockEx> BlockVector;
  typedef cryptonote::BasicUpgradeDetector<BlockVector> UpgradeDetector;

  cryptonote::Currency createCurrency(uint64_t upgradeHeight = UpgradeDetector::UNDEF_HEIGHT) {
    cryptonote::CurrencyBuilder currencyBuilder;
    currencyBuilder.upgradeVotingThreshold(90);
    currencyBuilder.upgradeVotingWindow(720);
    currencyBuilder.upgradeWindow(720);
    currencyBuilder.upgradeHeight(upgradeHeight);
    return currencyBuilder.currency();
  }

  void createBlocks(BlockVector& blockchain, size_t count, uint8_t majorVersion, uint8_t minorVersion) {
    for (size_t i = 0; i < count; ++i) {
      BlockEx b;
      b.bl.majorVersion = majorVersion;
      b.bl.minorVersion = minorVersion;
      b.bl.timestamp = 0;
      blockchain.push_back(b);
    }
  }

  void createBlocks(BlockVector& blockchain, UpgradeDetector& upgradeDetector, size_t count, uint8_t majorVersion, uint8_t minorVersion) {
    for (size_t i = 0; i < count; ++i) {
      BlockEx b;
      b.bl.majorVersion = majorVersion;
      b.bl.minorVersion = minorVersion;
      b.bl.timestamp = 0;
      blockchain.push_back(b);
      upgradeDetector.blockPushed();
    }
  }

  void popBlocks(BlockVector& blockchain, UpgradeDetector& upgradeDetector, size_t count) {
    for (size_t i = 0; i < count; ++i) {
      blockchain.pop_back();
      upgradeDetector.blockPopped();
    }
  }


  TEST(UpgradeDetector_voting_init, handlesEmptyBlockchain) {
    cryptonote::Currency currency = createCurrency();
    BlockVector blocks;
    UpgradeDetector upgradeDetector(currency, blocks, BLOCK_MAJOR_VERSION_2);
    ASSERT_TRUE(upgradeDetector.init());
    ASSERT_EQ(upgradeDetector.votingCompleteHeight(), UpgradeDetector::UNDEF_HEIGHT);
  }

  TEST(UpgradeDetector_voting_init, votingIsNotCompleteDueShortBlockchain) {
    cryptonote::Currency currency = createCurrency();
    BlockVector blocks;
    createBlocks(blocks, currency.upgradeVotingWindow() - 1, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1);

    UpgradeDetector upgradeDetector(currency, blocks, BLOCK_MAJOR_VERSION_2);
    ASSERT_TRUE(upgradeDetector.init());
    ASSERT_EQ(upgradeDetector.votingCompleteHeight(), UpgradeDetector::UNDEF_HEIGHT);
  }

  TEST(UpgradeDetector_voting_init, votingIsCompleteAfterMinimumNumberOfBlocks) {
    cryptonote::Currency currency = createCurrency();
    BlockVector blocks;
    createBlocks(blocks, currency.upgradeVotingWindow(), BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1);

    UpgradeDetector upgradeDetector(currency, blocks, BLOCK_MAJOR_VERSION_2);
    ASSERT_TRUE(upgradeDetector.init());
    ASSERT_EQ(upgradeDetector.votingCompleteHeight(), currency.upgradeVotingWindow() - 1);
  }

  TEST(UpgradeDetector_voting_init, votingIsNotCompleteDueLackOfVoices) {
    cryptonote::Currency currency = createCurrency();
    BlockVector blocks;
    createBlocks(blocks, currency.upgradeVotingWindow(), BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_0);
    createBlocks(blocks, currency.minNumberVotingBlocks() - 1, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1);

    UpgradeDetector upgradeDetector(currency, blocks, BLOCK_MAJOR_VERSION_2);
    ASSERT_TRUE(upgradeDetector.init());
    ASSERT_EQ(upgradeDetector.votingCompleteHeight(), UpgradeDetector::UNDEF_HEIGHT);
  }

  TEST(UpgradeDetector_voting_init, votingIsCompleteAfterMinimumNumberOfVoices) {
    cryptonote::Currency currency = createCurrency();
    BlockVector blocks;
    createBlocks(blocks, currency.upgradeVotingWindow(), BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_0);
    createBlocks(blocks, currency.minNumberVotingBlocks(), BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1);

    UpgradeDetector upgradeDetector(currency, blocks, BLOCK_MAJOR_VERSION_2);
    ASSERT_TRUE(upgradeDetector.init());
    ASSERT_EQ(upgradeDetector.votingCompleteHeight(), blocks.size() - 1);
  }

  TEST(UpgradeDetector_voting_init, handlesOneCompleteUpgrade) {
    cryptonote::Currency currency = createCurrency();
    BlockVector blocks;
    createBlocks(blocks, currency.upgradeVotingWindow(), BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1);
    uint64_t upgradeHeight = currency.calculateUpgradeHeight(blocks.size() - 1);
    createBlocks(blocks, upgradeHeight - blocks.size(), BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_0);
    // Upgrade is here
    createBlocks(blocks, 1, BLOCK_MAJOR_VERSION_2, BLOCK_MINOR_VERSION_0);

    UpgradeDetector upgradeDetector(currency, blocks, BLOCK_MAJOR_VERSION_2);
    ASSERT_TRUE(upgradeDetector.init());
    ASSERT_EQ(upgradeDetector.votingCompleteHeight(), currency.upgradeVotingWindow() - 1);
    ASSERT_EQ(upgradeDetector.upgradeHeight(), upgradeHeight);
  }

  TEST(UpgradeDetector_voting_init, handlesAFewCompleteUpgrades) {
    cryptonote::Currency currency = createCurrency();
    const uint8_t BLOCK_V3 = BLOCK_MAJOR_VERSION_2 + 1;
    const uint8_t BLOCK_V4 = BLOCK_MAJOR_VERSION_2 + 2;

    BlockVector blocks;

    createBlocks(blocks, currency.upgradeVotingWindow(), BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1);
    uint64_t votingCompleteHeigntV2 = blocks.size() - 1;
    uint64_t upgradeHeightV2 = currency.calculateUpgradeHeight(votingCompleteHeigntV2);
    createBlocks(blocks, upgradeHeightV2 - blocks.size(), BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_0);
    // Upgrade to v2 is here
    createBlocks(blocks, 1, BLOCK_MAJOR_VERSION_2, BLOCK_MINOR_VERSION_0);

    createBlocks(blocks, currency.upgradeVotingWindow(), BLOCK_MAJOR_VERSION_2, BLOCK_MINOR_VERSION_1);
    uint64_t votingCompleteHeigntV3 = blocks.size() - 1;
    uint64_t upgradeHeightV3 = currency.calculateUpgradeHeight(votingCompleteHeigntV3);
    createBlocks(blocks, upgradeHeightV3 - blocks.size(), BLOCK_MAJOR_VERSION_2, BLOCK_MINOR_VERSION_0);
    // Upgrade to v3 is here
    createBlocks(blocks, 1, BLOCK_V3, BLOCK_MINOR_VERSION_0);

    createBlocks(blocks, currency.upgradeVotingWindow(), BLOCK_V3, BLOCK_MINOR_VERSION_1);
    uint64_t votingCompleteHeigntV4 = blocks.size() - 1;
    uint64_t upgradeHeightV4 = currency.calculateUpgradeHeight(votingCompleteHeigntV4);
    createBlocks(blocks, upgradeHeightV4 - blocks.size(), BLOCK_V3, BLOCK_MINOR_VERSION_0);
    // Upgrade to v4 is here
    createBlocks(blocks, 1, BLOCK_V4, BLOCK_MINOR_VERSION_0);

    UpgradeDetector upgradeDetectorV2(currency, blocks, BLOCK_MAJOR_VERSION_2);
    ASSERT_TRUE(upgradeDetectorV2.init());
    ASSERT_EQ(upgradeDetectorV2.votingCompleteHeight(), votingCompleteHeigntV2);
    ASSERT_EQ(upgradeDetectorV2.upgradeHeight(), upgradeHeightV2);

    UpgradeDetector upgradeDetectorV3(currency, blocks, BLOCK_V3);
    ASSERT_TRUE(upgradeDetectorV3.init());
    ASSERT_EQ(upgradeDetectorV3.votingCompleteHeight(), votingCompleteHeigntV3);
    ASSERT_EQ(upgradeDetectorV3.upgradeHeight(), upgradeHeightV3);

    UpgradeDetector upgradeDetectorV4(currency, blocks, BLOCK_V4);
    ASSERT_TRUE(upgradeDetectorV4.init());
    ASSERT_EQ(upgradeDetectorV4.votingCompleteHeight(), votingCompleteHeigntV4);
    ASSERT_EQ(upgradeDetectorV4.upgradeHeight(), upgradeHeightV4);
  }

  TEST(UpgradeDetector_upgradeHeight_init, handlesEmptyBlockchain) {
    const uint64_t upgradeHeight = 17;
    cryptonote::Currency currency = createCurrency(upgradeHeight);
    BlockVector blocks;
    UpgradeDetector upgradeDetector(currency, blocks, BLOCK_MAJOR_VERSION_2);
    ASSERT_TRUE(upgradeDetector.init());
    ASSERT_EQ(upgradeDetector.upgradeHeight(), upgradeHeight);
    ASSERT_EQ(upgradeDetector.votingCompleteHeight(), UpgradeDetector::UNDEF_HEIGHT);
  }

  TEST(UpgradeDetector_upgradeHeight_init, handlesBlockchainBeforeUpgrade) {
    const uint64_t upgradeHeight = 17;
    cryptonote::Currency currency = createCurrency(upgradeHeight);
    BlockVector blocks;
    createBlocks(blocks, upgradeHeight, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1);

    UpgradeDetector upgradeDetector(currency, blocks, BLOCK_MAJOR_VERSION_2);
    ASSERT_TRUE(upgradeDetector.init());
    ASSERT_EQ(upgradeDetector.upgradeHeight(), upgradeHeight);
    ASSERT_EQ(upgradeDetector.votingCompleteHeight(), UpgradeDetector::UNDEF_HEIGHT);
  }

  TEST(UpgradeDetector_upgradeHeight_init, handlesBlockchainAtUpgrade) {
    const uint64_t upgradeHeight = 17;
    cryptonote::Currency currency = createCurrency(upgradeHeight);
    BlockVector blocks;
    createBlocks(blocks, upgradeHeight + 1, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1);

    UpgradeDetector upgradeDetector(currency, blocks, BLOCK_MAJOR_VERSION_2);
    ASSERT_TRUE(upgradeDetector.init());
    ASSERT_EQ(upgradeDetector.upgradeHeight(), upgradeHeight);
    ASSERT_EQ(upgradeDetector.votingCompleteHeight(), UpgradeDetector::UNDEF_HEIGHT);
  }

  TEST(UpgradeDetector_upgradeHeight_init, handlesBlockchainAfterUpgrade) {
    const uint64_t upgradeHeight = 17;
    cryptonote::Currency currency = createCurrency(upgradeHeight);
    BlockVector blocks;
    createBlocks(blocks, upgradeHeight + 1, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1);
    createBlocks(blocks, 1, BLOCK_MAJOR_VERSION_2, BLOCK_MINOR_VERSION_0);

    UpgradeDetector upgradeDetector(currency, blocks, BLOCK_MAJOR_VERSION_2);
    ASSERT_TRUE(upgradeDetector.init());
    ASSERT_EQ(upgradeDetector.upgradeHeight(), upgradeHeight);
    ASSERT_EQ(upgradeDetector.votingCompleteHeight(), UpgradeDetector::UNDEF_HEIGHT);
  }

  TEST(UpgradeDetector_voting, handlesVotingCompleteStartingEmptyBlockchain) {
    cryptonote::Currency currency = createCurrency();
    BlockVector blocks;
    UpgradeDetector upgradeDetector(currency, blocks, BLOCK_MAJOR_VERSION_2);
    ASSERT_TRUE(upgradeDetector.init());

    createBlocks(blocks, upgradeDetector, currency.upgradeVotingWindow(), BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_0);
    createBlocks(blocks, upgradeDetector, currency.minNumberVotingBlocks(), BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1);
    ASSERT_EQ(upgradeDetector.votingCompleteHeight(), blocks.size() - 1);
  }

  TEST(UpgradeDetector_voting, handlesVotingCompleteStartingNonEmptyBlockchain) {
    cryptonote::Currency currency = createCurrency();
    assert(currency.minNumberVotingBlocks() >= 2);
    const uint64_t portion = currency.minNumberVotingBlocks() - currency.minNumberVotingBlocks() / 2;

    BlockVector blocks;
    UpgradeDetector upgradeDetector(currency, blocks, BLOCK_MAJOR_VERSION_2);

    createBlocks(blocks, upgradeDetector, currency.upgradeVotingWindow(), BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_0);
    createBlocks(blocks, upgradeDetector, currency.minNumberVotingBlocks() - portion, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1);

    ASSERT_TRUE(upgradeDetector.init());
    createBlocks(blocks, upgradeDetector, portion, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1);
    ASSERT_EQ(upgradeDetector.votingCompleteHeight(), blocks.size() - 1);
  }

  TEST(UpgradeDetector_voting, handlesVotingCancelling) {
    cryptonote::Currency currency = createCurrency();
    BlockVector blocks;
    UpgradeDetector upgradeDetector(currency, blocks, BLOCK_MAJOR_VERSION_2);
    ASSERT_TRUE(upgradeDetector.init());

    createBlocks(blocks, upgradeDetector, currency.upgradeVotingWindow(), BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_0);
    createBlocks(blocks, upgradeDetector, currency.minNumberVotingBlocks(), BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1);
    uint64_t votingCompleteHeight = blocks.size() - 1;
    uint64_t hadrforkHeight = currency.calculateUpgradeHeight(votingCompleteHeight);
    ASSERT_EQ(upgradeDetector.votingCompleteHeight(), votingCompleteHeight);

    createBlocks(blocks, upgradeDetector, hadrforkHeight - votingCompleteHeight - 1, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_0);
    ASSERT_EQ(upgradeDetector.votingCompleteHeight(), votingCompleteHeight);

    // Cancel voting
    popBlocks(blocks, upgradeDetector, hadrforkHeight - votingCompleteHeight - 1);
    ASSERT_EQ(upgradeDetector.votingCompleteHeight(), votingCompleteHeight);
    popBlocks(blocks, upgradeDetector, 1);
    ASSERT_EQ(upgradeDetector.votingCompleteHeight(), UpgradeDetector::UNDEF_HEIGHT);
  }

  TEST(UpgradeDetector_voting, handlesVotingAndUpgradeCancelling) {
    cryptonote::Currency currency = createCurrency();
    BlockVector blocks;
    UpgradeDetector upgradeDetector(currency, blocks, BLOCK_MAJOR_VERSION_2);
    ASSERT_TRUE(upgradeDetector.init());

    createBlocks(blocks, upgradeDetector, currency.upgradeVotingWindow(), BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_0);
    createBlocks(blocks, upgradeDetector, currency.minNumberVotingBlocks(), BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1);
    uint64_t votingCompleteHeight = blocks.size() - 1;
    uint64_t hadrforkHeight = currency.calculateUpgradeHeight(votingCompleteHeight);
    ASSERT_EQ(votingCompleteHeight, upgradeDetector.votingCompleteHeight());

    createBlocks(blocks, upgradeDetector, hadrforkHeight - votingCompleteHeight, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_0);
    createBlocks(blocks, upgradeDetector, 1, BLOCK_MAJOR_VERSION_2, BLOCK_MINOR_VERSION_0);
    ASSERT_EQ(votingCompleteHeight, upgradeDetector.votingCompleteHeight());

    // Cancel upgrade (pop block v2)
    popBlocks(blocks, upgradeDetector, 1);
    ASSERT_EQ(votingCompleteHeight, upgradeDetector.votingCompleteHeight());

    // Pop blocks after voting
    popBlocks(blocks, upgradeDetector, hadrforkHeight - votingCompleteHeight);
    ASSERT_EQ(votingCompleteHeight, upgradeDetector.votingCompleteHeight());

    // Cancel voting
    popBlocks(blocks, upgradeDetector, 1);
    ASSERT_EQ(UpgradeDetector::UNDEF_HEIGHT, upgradeDetector.votingCompleteHeight());
  }
}
