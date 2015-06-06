// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "upgrade.h"
#include "TestGenerator.h"

using namespace epee;
using namespace cryptonote;

using std::size_t;

namespace {
  bool makeBlocks(std::vector<test_event_entry>& events, test_generator& generator, Block& lastBlock,
                  const Block& parentBlock, const cryptonote::account_base& minerAcc, size_t count,
                  uint8_t majorVersion, uint8_t minorVersion) {
    cryptonote::Block prevBlock = parentBlock;
    for (size_t i = 0; i < count; ++i) {
      cryptonote::Block b;
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
  bool makeBlockTxV2(std::vector<test_event_entry>& events, test_generator& generator, Block& lastBlock,
                  const Block& parentBlock, const cryptonote::account_base& minerAcc, const cryptonote::account_base& to, size_t count,
                  uint8_t majorVersion, uint8_t minorVersion, bool before = true) {
    cryptonote::Block prevBlock = parentBlock;
    cryptonote::Block b;
    auto currency = CurrencyBuilder().currency();
    TestGenerator gen(generator, minerAcc, parentBlock, currency, events);
    Transaction transaction;
    auto builder = gen.createTxBuilder(minerAcc, to, currency.depositMinAmount(), currency.minimumFee());
    builder.m_destinations.clear();

    TransactionBuilder::KeysVector kv;
    kv.push_back(to.get_keys());

    builder.addMultisignatureOut(currency.depositMinAmount(), kv, 1, currency.depositMinTerm());
    builder.setVersion(TRANSACTION_VERSION_2);
    auto tx = builder.build();
    if (before) gen.addCallback("markInvalidBlock"); // should be rejected by the core
    gen.addEvent(tx);
    if (before) gen.addCallback("markInvalidBlock"); // should be rejected by the core
    gen.makeNextBlock(tx);
    lastBlock = gen.lastBlock;
    return true;
  }
  bool makeBlockTxV1(std::vector<test_event_entry>& events, test_generator& generator, Block& lastBlock,
                  const Block& parentBlock, const cryptonote::account_base& minerAcc, const cryptonote::account_base& to, size_t count,
                  uint8_t majorVersion, uint8_t minorVersion) {
    cryptonote::Block prevBlock = parentBlock;
    cryptonote::Block b;
    auto currency = CurrencyBuilder().currency();
    TestGenerator gen(generator, minerAcc, parentBlock, currency, events);
    Transaction transaction;
    auto builder = gen.createTxBuilder(minerAcc, to, currency.depositMinAmount(), currency.minimumFee());
    builder.m_destinations.clear();

    TransactionBuilder::KeysVector kv;
    kv.push_back(to.get_keys());

    builder.addOutput({currency.depositMinAmount(), to.get_keys().m_account_address});
    builder.setVersion(TRANSACTION_VERSION_1);
    auto tx = builder.build();
    gen.addEvent(tx);
    gen.makeNextBlock(tx);
    lastBlock = gen.lastBlock;
    return true;
  }
}

gen_upgrade::gen_upgrade() : m_invalidBlockIndex(0), m_checkBlockTemplateVersionCallCounter(0),
    m_coinsInCirculationBeforeUpgrade(0), m_coinsInCirculationAfterUpgrade(0) {
  cryptonote::CurrencyBuilder currencyBuilder;
  currencyBuilder.maxBlockSizeInitial(std::numeric_limits<size_t>::max() / 2);
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
                                     const cryptonote::Block& parentBlock, const cryptonote::account_base& minerAcc,
                                     bool checkReward) const {
  Block blk;
  //DO_CALLBACK(events, "markInvalidBlock");
  if (!makeBlockTxV2(events, generator, blk, parentBlock, minerAcc, to, 1, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1)) {
    return false;
  }

  // Checking 1: get_block_templare returns block with major version 1
  DO_CALLBACK(events, "checkBlockTemplateVersionIsV1");

  // Checking 2: block with version 2.0 doesn't accepted
  Block badBlock;
  DO_CALLBACK(events, "markInvalidBlock");
  return makeBlocks(events, generator, badBlock, parentBlock, minerAcc, 1, BLOCK_MAJOR_VERSION_2, BLOCK_MINOR_VERSION_0);
}

bool gen_upgrade::checkAfterUpgrade(std::vector<test_event_entry>& events, test_generator& generator,
                                    const cryptonote::Block& parentBlock, const cryptonote::account_base& minerAcc) const {
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

bool gen_upgrade::check_block_verification_context(const cryptonote::block_verification_context& bvc, size_t eventIdx, const cryptonote::Block& /*blk*/) {
  if (m_invalidBlockIndex == eventIdx) {
    m_invalidBlockIndex = 0;
    return bvc.m_verifivation_failed;
  } else {
    return !bvc.m_verifivation_failed;
  }
}

bool gen_upgrade::markInvalidBlock(cryptonote::core& /*c*/, size_t evIndex, const std::vector<test_event_entry>& /*events*/) {
  m_invalidBlockIndex = evIndex + 1;
  return true;
}

bool gen_upgrade::checkBlockTemplateVersionIsV1(cryptonote::core& c, size_t /*evIndex*/, const std::vector<test_event_entry>& /*events*/) {
  DEFINE_TESTS_ERROR_CONTEXT("gen_upgrade::checkBlockTemplateVersionIsV1");
  CHECK_TEST_CONDITION(checkBlockTemplateVersion(c, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1));
  return true;
}

bool gen_upgrade::checkBlockTemplateVersionIsV2(cryptonote::core& c, size_t /*evIndex*/, const std::vector<test_event_entry>& /*events*/) {
  DEFINE_TESTS_ERROR_CONTEXT("gen_upgrade::checkBlockTemplateVersionIsV2");
  CHECK_TEST_CONDITION(checkBlockTemplateVersion(c, BLOCK_MAJOR_VERSION_2, BLOCK_MINOR_VERSION_0));
  return true;
}

bool gen_upgrade::checkBlockTemplateVersion(cryptonote::core& c, uint8_t expectedMajorVersion, uint8_t expectedMinorVersion) {
  DEFINE_TESTS_ERROR_CONTEXT("gen_upgrade::checkBlockTemplateVersion");

  account_base account;
  account.generate();

  Block b;
  difficulty_type diff;
  uint64_t height;
  CHECK_TEST_CONDITION(c.get_block_template(b, account.get_keys().m_account_address, diff, height, blobdata()));
  CHECK_EQ(b.majorVersion, expectedMajorVersion);
  CHECK_EQ(b.minorVersion, expectedMinorVersion);

  return true;
}
