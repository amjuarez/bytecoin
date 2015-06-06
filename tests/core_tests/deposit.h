// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "chaingen.h"
#include "TestGenerator.h"
#include <cryptonote_core/cryptonote_basic.h>
#include <cryptonote_core/Currency.h>

namespace DepositTests {
struct DepositTestsBase : public test_chain_unit_base {
  DepositTestsBase() {
    m_currency =
        cryptonote::CurrencyBuilder().upgradeHeight(0).depositMinTerm(10).depositMinTotalRateFactor(100).currency();
    from.generate();
    to.generate();
    REGISTER_CALLBACK_METHOD(DepositTestsBase, mark_invalid_block);
    REGISTER_CALLBACK_METHOD(DepositTestsBase, mark_invalid_tx);
    REGISTER_CALLBACK_METHOD(DepositTestsBase, check_emission);
  }
  cryptonote::Transaction createDepositTransaction(std::vector<test_event_entry>& events);

  bool mark_invalid_tx(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);

  bool check_emission(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);

  // TransactionBuilder::MultisignatureSource createSource(uint32_t term = 0) const;
  TransactionBuilder::MultisignatureSource createSource(uint32_t term, cryptonote::KeyPair key) const;

  bool check_tx_verification_context(const cryptonote::tx_verification_context& tvc, bool tx_added,
                                     std::size_t event_idx, const cryptonote::Transaction& /*tx*/);

  bool check_block_verification_context(const cryptonote::block_verification_context& bvc, size_t eventIdx,
                                        const cryptonote::Block& /*blk*/) {
    if (blockId == eventIdx) {
      return bvc.m_verifivation_failed;
    } else {
      return !bvc.m_verifivation_failed;
    }
  }

  bool mark_invalid_block(cryptonote::core& /*c*/, size_t ev_index, const std::vector<test_event_entry>& /*events*/) {
    blockId = ev_index + 1;
    return true;
  }

  TestGenerator prepare(std::vector<test_event_entry>& events) const;
  void addDepositOutput(cryptonote::Transaction& transaction);
  void addDepositInput(cryptonote::Transaction& transaction);
  cryptonote::Transaction transaction;
  cryptonote::account_base from;
  cryptonote::account_base to;
  std::size_t blockId = 0;
  std::size_t emission = 0;
};

struct EmissionTest : public DepositTestsBase {
  EmissionTest() {
    m_currency =
        cryptonote::CurrencyBuilder().upgradeHeight(0).depositMinTerm(10).depositMinTotalRateFactor(100).currency();
    REGISTER_CALLBACK_METHOD(EmissionTest, save_emission_before);
    REGISTER_CALLBACK_METHOD(EmissionTest, save_emission_after);
  }
  bool check_block_verification_context(const cryptonote::block_verification_context& bvc, std::size_t eventIdx,
                                        const cryptonote::Block& /*blk*/) {
    if (emission_after == 0 || emission_before == 0) {
      return true;
    }
    return emission_after ==
           emission_before + cryptonote::START_BLOCK_REWARD +
               m_currency.calculateInterest(m_currency.depositMinAmount(), m_currency.depositMinTerm());
  }

  bool save_emission_before(cryptonote::core& c, std::size_t /*ev_index*/,
                            const std::vector<test_event_entry>& /*events*/) {
    emission_before = c.getCoinsInCirculation();
    return true;
  }

  bool save_emission_after(cryptonote::core& c, std::size_t ev_index, const std::vector<test_event_entry>& /*events*/) {
    emission_after = c.getCoinsInCirculation();
    return true;
  }
  TestGenerator prepare(std::vector<test_event_entry>& events) const;
  std::size_t emission_before = 0;
  std::size_t emission_after = 0;
};

struct EmissionTestRestore : public EmissionTest {
  bool check_block_verification_context(const cryptonote::block_verification_context& bvc, std::size_t eventIdx,
                                        const cryptonote::Block& /*blk*/) {
    if (emission_after == 0 || emission_before == 0) {
      return true;
    }
    return emission_after ==
           emission_before + cryptonote::START_BLOCK_REWARD * 3 -
               m_currency.calculateInterest(m_currency.depositMinAmount(), m_currency.depositMinTerm());
  }
};

struct BlocksOfFirstTypeCantHaveTransactionsOfTypeTwo : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};

struct BlocksOfSecondTypeCanHaveTransactionsOfTypeOne : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};

struct BlocksOfSecondTypeCanHaveTransactionsOfTypeTwo : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionOfTypeOneWithDepositInputIsRejected : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionOfTypeOneWithDepositOutputIsRejected : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};

//
struct TransactionWithAmountLowerThenMinIsRejected : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithMinAmountIsAccepted : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithTermLowerThenMinIsRejected : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithMinTermIsAccepted : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithTermGreaterThenMaxIsRejected : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithMaxTermIsAccepted : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithoutSignaturesIsRejected : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithZeroRequiredSignaturesIsRejected : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithNumberOfRequiredSignaturesGreaterThanKeysIsRejected : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithInvalidKeyIsRejected : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithDepositExtendsEmission : public EmissionTest {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithDepositRestorsEmissionOnAlternativeChain : public EmissionTestRestore {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithOutputToSpentInputWillBeRejected : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithMultipleInputsThatSpendOneOutputWillBeRejected : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithInputWithAmountThatIsDoesntHaveOutputWithSameAmountWillBeRejected
    : public DepositTestsBase { // hello, java, I missed you...
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithInputWithIndexLargerThanNumberOfOutputsWithThisSumWillBeRejected : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithInputThatPointsToTheOutputButHasAnotherTermWillBeRejected : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionThatTriesToSpendOutputWhosTermHasntFinishedWillBeRejected : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithAmountThatHasAlreadyFinishedWillBeAccepted : public DepositTestsBase {
  bool generate(std::vector<test_event_entry>& events);
};
}
