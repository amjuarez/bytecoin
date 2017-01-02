// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2017 XDN-project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "Upgrade.h"
#include "TestGenerator.h"

using namespace CryptoNote;

namespace {
  bool makeBlocks(std::vector<test_event_entry>& events, test_generator& generator, Block& lastBlock,
                  const Block& parentBlock, const CryptoNote::AccountBase& minerAcc, size_t count,
                  uint8_t majorVersion, uint8_t minorVersion) {
    CryptoNote::Block prevBlock = parentBlock;
    for (size_t i = 0; i < count; ++i) {
      CryptoNote::Block b;
      bool r = generator.constructBlockManually(b, prevBlock, minerAcc, test_generator::bf_major_ver | test_generator::bf_minor_ver,
        majorVersion, minorVersion);
      if (!r) {
        return false;
      } else {
        prevBlock = b;
        events.push_back(b);
      }
    }

    lastBlock = prevBlock;

    return true;
  }
}

gen_upgrade::gen_upgrade() : m_invalidBlockIndex(0), m_checkBlockTemplateVersionCallCounter(0) {
  CryptoNote::CurrencyBuilder currencyBuilder(m_logger);
  currencyBuilder.maxBlockSizeInitial(std::numeric_limits<size_t>::max() / 2);
  currencyBuilder.upgradeHeightV2(UpgradeDetectorBase::UNDEF_HEIGHT);
  // Disable voting and never upgrade to v.3.0
  currencyBuilder.upgradeHeightV3(CryptoNote::parameters::CRYPTONOTE_MAX_BLOCK_NUMBER);
  m_currency = currencyBuilder.currency();

  REGISTER_CALLBACK_METHOD(gen_upgrade, markInvalidBlock);
  REGISTER_CALLBACK_METHOD(gen_upgrade, checkBlockTemplateVersionIsV1);
  REGISTER_CALLBACK_METHOD(gen_upgrade, checkBlockTemplateVersionIsV2);
}

bool gen_upgrade::generate(std::vector<test_event_entry>& events) const {
  const uint64_t tsStart = 1338224400;

  GENERATE_ACCOUNT(minerAccount);
  GENERATE_ACCOUNT(to);
  MAKE_GENESIS_BLOCK(events, blk0, minerAccount, tsStart);

  // Vote for upgrade
  Block blk1;
  if (!makeBlocks(events, generator, blk1, blk0, minerAccount, m_currency.minNumberVotingBlocks(), BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1)) {
    return false;
  }

  if (!checkBeforeUpgrade(events, generator, blk1, minerAccount, true)) {
    return false;
  }

  // Fill m_currency.upgradeVotingWindow()
  Block blk2;
  if (!makeBlocks(events, generator, blk2, blk1, minerAccount, m_currency.upgradeVotingWindow() - m_currency.minNumberVotingBlocks() - 1,
      BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_0)) {
    return false;
  }

  // Upgrade voting complete!
  uint64_t votingCompleteHeight = get_block_height(blk2);
  uint64_t upgradeHeight = m_currency.calculateUpgradeHeight(votingCompleteHeight);

  if (!checkBeforeUpgrade(events, generator, blk2, minerAccount, true)) {
    return false;
  }

  // Create blocks up to upgradeHeight
  Block blk3;
  if (!makeBlocks(events, generator, blk3, blk2, minerAccount, upgradeHeight - votingCompleteHeight - 1, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_0)) {
    return false;
  }

  if (!checkBeforeUpgrade(events, generator, blk3, minerAccount, false)) {
    return false;
  }

  // Create last block with version 1.x
  Block blk4;
  if (!makeBlocks(events, generator, blk4, blk3, minerAccount, 1, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_0)) {
    return false;
  }

  generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.defaultMinorVersion = BLOCK_MINOR_VERSION_0;

  if (!checkAfterUpgrade(events, generator, blk4, minerAccount)) {
    return false;
  }

  // Create a few blocks with version 2.0
  Block blk5;
  if (!makeBlocks(events, generator, blk5, blk4, minerAccount, 3, BLOCK_MAJOR_VERSION_2, BLOCK_MINOR_VERSION_0)) {
    return false;
  }

  if (!checkAfterUpgrade(events, generator, blk5, minerAccount)) {
    return false;
  }

  return true;
}

bool gen_upgrade::checkBeforeUpgrade(std::vector<test_event_entry>& events, test_generator& generator,
                                     const CryptoNote::Block& parentBlock, const CryptoNote::AccountBase& minerAcc,
                                     bool checkReward) const {
  // Checking 1: transactions with version 2.0 doesn't accepted
  Block blk;
  if (!makeBlockTxV2(events, generator, blk, parentBlock, minerAcc, to, 1, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1)) {
    return false;
  }

  // Checking 2: get_block_templare returns block with major version 1
  DO_CALLBACK(events, "checkBlockTemplateVersionIsV1");

  // Checking 3: block with version 2.0 doesn't accepted
  Block badBlock;
  DO_CALLBACK(events, "markInvalidBlock");
  return makeBlocks(events, generator, badBlock, parentBlock, minerAcc, 1, BLOCK_MAJOR_VERSION_2, BLOCK_MINOR_VERSION_0);
}

bool gen_upgrade::checkAfterUpgrade(std::vector<test_event_entry>& events, test_generator& generator,
                                    const CryptoNote::Block& parentBlock, const CryptoNote::AccountBase& minerAcc) const {
  // Checking 1: get_block_templare returns block with major version 2
  DO_CALLBACK(events, "checkBlockTemplateVersionIsV2");

  // Checking 2: block with version 1.0 doesn't accepted
  Block badBlock;
  DO_CALLBACK(events, "markInvalidBlock");
  if (!makeBlocks(events, generator, badBlock, parentBlock, minerAcc, 1, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_0)) {
    return false;
  }

  Block blk1;
  if (!makeBlockTxV1(events, generator, blk1, parentBlock, minerAcc, to, m_currency.minNumberVotingBlocks(), BLOCK_MAJOR_VERSION_2, BLOCK_MINOR_VERSION_0)) {
    return false;
  }

  Block blk2;
  if (!makeBlockTxV2(events, generator, blk2, parentBlock, minerAcc, to, m_currency.minNumberVotingBlocks(), BLOCK_MAJOR_VERSION_2, BLOCK_MINOR_VERSION_0, false)) {
    return false;
  }

  // Checking 3: block with version 1.1 doesn't accepted
  DO_CALLBACK(events, "markInvalidBlock");
  return makeBlocks(events, generator, badBlock, parentBlock, minerAcc, 1, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1);
}

bool gen_upgrade::check_block_verification_context(const CryptoNote::block_verification_context& bvc, size_t eventIdx, const CryptoNote::Block& /*blk*/) {
  if (m_invalidBlockIndex == eventIdx) {
    m_invalidBlockIndex = 0;
    return bvc.m_verifivation_failed;
  } else {
    return !bvc.m_verifivation_failed;
  }
}

bool gen_upgrade::markInvalidBlock(CryptoNote::core& /*c*/, size_t evIndex, const std::vector<test_event_entry>& /*events*/) {
  m_invalidBlockIndex = evIndex + 1;
  return true;
}

bool gen_upgrade::checkBlockTemplateVersionIsV1(CryptoNote::core& c, size_t /*evIndex*/, const std::vector<test_event_entry>& /*events*/) {
  DEFINE_TESTS_ERROR_CONTEXT("gen_upgrade::checkBlockTemplateVersionIsV1");
  CHECK_TEST_CONDITION(checkBlockTemplateVersion(c, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1));
  return true;
}

bool gen_upgrade::checkBlockTemplateVersionIsV2(CryptoNote::core& c, size_t /*evIndex*/, const std::vector<test_event_entry>& /*events*/) {
  DEFINE_TESTS_ERROR_CONTEXT("gen_upgrade::checkBlockTemplateVersionIsV2");
  CHECK_TEST_CONDITION(checkBlockTemplateVersion(c, BLOCK_MAJOR_VERSION_2, BLOCK_MINOR_VERSION_0));
  return true;
}

bool gen_upgrade::checkBlockTemplateVersion(CryptoNote::core& c, uint8_t expectedMajorVersion, uint8_t expectedMinorVersion) {
  DEFINE_TESTS_ERROR_CONTEXT("gen_upgrade::checkBlockTemplateVersion");

  AccountBase account;
  account.generate();

  Block b;
  difficulty_type diff;
  uint32_t height;
  CHECK_TEST_CONDITION(c.get_block_template(b, account.getAccountKeys().address, diff, height, BinaryArray()));
  CHECK_EQ(static_cast<int>(b.majorVersion), static_cast<int>(expectedMajorVersion));
  CHECK_EQ(static_cast<int>(b.minorVersion), static_cast<int>(expectedMinorVersion));

  return true;
}

bool gen_upgrade::makeBlockTxV1(std::vector<test_event_entry>& events, test_generator& generator, Block& lastBlock,
                                const Block& parentBlock, const AccountBase& minerAcc, const AccountBase& to, size_t count,
                                uint8_t majorVersion, uint8_t minorVersion) const {
  Block prevBlock = parentBlock;
  Block b;
  TestGenerator gen(generator, minerAcc, parentBlock, m_currency, events);
  Transaction transaction;
  auto builder = gen.createTxBuilder(minerAcc, to, m_currency.depositMinAmount(), m_currency.minimumFee());
  builder.m_destinations.clear();

  TransactionBuilder::KeysVector kv;
  kv.push_back(to.getAccountKeys());

  builder.addOutput({ m_currency.depositMinAmount(), to.getAccountKeys().address });
  builder.setVersion(TRANSACTION_VERSION_1);
  auto tx = builder.build();
  gen.addEvent(tx);
  gen.makeNextBlock(tx);
  lastBlock = gen.lastBlock;
  return true;
}

bool gen_upgrade::makeBlockTxV2(std::vector<test_event_entry>& events, test_generator& generator, Block& lastBlock,
                                const Block& parentBlock, const AccountBase& minerAcc, const AccountBase& to, size_t count,
                                uint8_t majorVersion, uint8_t minorVersion, bool before) const {
  Block prevBlock = parentBlock;
  Block b;
  TestGenerator gen(generator, minerAcc, parentBlock, m_currency, events);
  Transaction transaction;
  auto builder = gen.createTxBuilder(minerAcc, to, m_currency.depositMinAmount(), m_currency.minimumFee());
  builder.m_destinations.clear();

  TransactionBuilder::KeysVector kv;
  kv.push_back(to.getAccountKeys());

  builder.addMultisignatureOut(m_currency.depositMinAmount(), kv, 1, m_currency.depositMinTerm());
  builder.setVersion(TRANSACTION_VERSION_2);
  auto tx = builder.build();
  //if (before) gen.addCallback("markInvalidBlock"); // should be rejected by the core
  gen.addEvent(tx);
  if (before) gen.addCallback("markInvalidBlock"); // should be rejected by the core
  gen.makeNextBlock(tx);
  lastBlock = gen.lastBlock;
  return true;
}
