// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2017 XDN-project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "Chaingen.h"
#include "TestGenerator.h"
#include <CryptoNoteCore/CryptoNoteBasic.h>
#include <CryptoNoteCore/Currency.h>
#include <Logging/ConsoleLogger.h>

namespace DepositTests {
struct DepositTestsBase : public test_chain_unit_base {
  DepositTestsBase() {
    m_currency = CryptoNote::CurrencyBuilder(m_logger).upgradeHeightV2(0).depositMinTerm(10).depositMinTotalRateFactor(100).currency();
    from.generate();
    to.generate();
    REGISTER_CALLBACK_METHOD(DepositTestsBase, mark_invalid_block);
    REGISTER_CALLBACK_METHOD(DepositTestsBase, mark_invalid_tx);
    REGISTER_CALLBACK_METHOD(DepositTestsBase, check_emission);
  }
  CryptoNote::Transaction createDepositTransaction(std::vector<test_event_entry>& events);

  bool mark_invalid_tx(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);

  bool check_emission(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);

  // TransactionBuilder::MultisignatureSource createSource(uint32_t term = 0) const;
  TransactionBuilder::MultisignatureSource createSource(uint32_t term, CryptoNote::KeyPair key) const;

  bool check_tx_verification_context(const CryptoNote::tx_verification_context& tvc, bool tx_added,
                                     std::size_t event_idx, const CryptoNote::Transaction& /*tx*/);

  bool check_block_verification_context(const CryptoNote::block_verification_context& bvc, size_t eventIdx,
                                        const CryptoNote::Block& /*blk*/) {
    if (blockId == eventIdx) {
      return bvc.m_verifivation_failed;
    } else {
      return !bvc.m_verifivation_failed;
    }
  }

  bool mark_invalid_block(CryptoNote::core& /*c*/, size_t ev_index, const std::vector<test_event_entry>& /*events*/) {
    blockId = ev_index + 1;
    return true;
  }

  TestGenerator prepare(std::vector<test_event_entry>& events) const;
  void addDepositOutput(CryptoNote::Transaction& transaction);
  void addDepositInput(CryptoNote::Transaction& transaction);

  Logging::ConsoleLogger m_logger;
  CryptoNote::Transaction transaction;
  CryptoNote::AccountBase from;
  CryptoNote::AccountBase to;
  std::size_t blockId = 0;
  std::size_t emission = 0;
};

struct DepositIndexTest : public DepositTestsBase {
  using Block = CryptoNote::Block;
  using Core = CryptoNote::core;
  using Events = std::vector<test_event_entry>;
  DepositIndexTest() {
    m_currency = CryptoNote::CurrencyBuilder(m_logger).upgradeHeightV2(0).depositMinTerm(10).depositMinTotalRateFactor(100).mininumFee(1000).currency();
    REGISTER_CALLBACK_METHOD(DepositIndexTest, interestZero);
    REGISTER_CALLBACK_METHOD(DepositIndexTest, interestOneMinimal);
    REGISTER_CALLBACK_METHOD(DepositIndexTest, interestTwoMininmal);
    REGISTER_CALLBACK_METHOD(DepositIndexTest, amountZero);
    REGISTER_CALLBACK_METHOD(DepositIndexTest, amountOneMinimal);
    REGISTER_CALLBACK_METHOD(DepositIndexTest, amountThreeMinimal);
  }

  bool amountZero(const Core& c, std::size_t ev_index, const Events& events) {
    return c.fullDepositAmount() == 0;
  }

  bool amountOneMinimal(const Core& c, std::size_t ev_index, const Events& events) {
    return c.fullDepositAmount() == m_currency.depositMinAmount();
  }

  bool amountThreeMinimal(const Core& c, std::size_t ev_index, const Events& events) {
    return c.fullDepositAmount() == 3 * m_currency.depositMinAmount();
  }

  bool interestZero(const Core& c, std::size_t ev_index, const Events& events) {
    return c.fullDepositInterest() == 0;
  }

  bool interestOneMinimal(const Core& c, std::size_t ev_index, const Events& events) {
    return c.fullDepositInterest() == m_currency.calculateInterest(m_currency.depositMinAmount(), m_currency.depositMinTerm());
  }

  bool interestTwoMininmal(const Core& c, std::size_t ev_index, const Events& events) {
    return c.fullDepositInterest() == 2 * m_currency.calculateInterest(m_currency.depositMinAmount(), m_currency.depositMinTerm());
  }
};

struct EmissionTest : public DepositTestsBase {
  EmissionTest() {
    m_currency = CryptoNote::CurrencyBuilder(m_logger).upgradeHeightV2(0).depositMinTerm(10).depositMinTotalRateFactor(100).currency();
    REGISTER_CALLBACK_METHOD(EmissionTest, save_emission_before);
    REGISTER_CALLBACK_METHOD(EmissionTest, save_emission_after);
  }
  bool check_block_verification_context(const CryptoNote::block_verification_context& bvc, std::size_t eventIdx,
                                        const CryptoNote::Block& /*blk*/) {
    if (emission_after == 0 || emission_before == 0) {
      return true;
    }
    return emission_after == emission_before + CryptoNote::START_BLOCK_REWARD + m_currency.calculateInterest(m_currency.depositMinAmount(), m_currency.depositMinTerm());
  }

  bool save_emission_before(CryptoNote::core& c, std::size_t /*ev_index*/,
                            const std::vector<test_event_entry>& /*events*/) {
    emission_before = c.getTotalGeneratedAmount();
    return emission_before > 0;
  }

  bool save_emission_after(CryptoNote::core& c, std::size_t ev_index, const std::vector<test_event_entry>& /*events*/) {
    emission_after = c.getTotalGeneratedAmount();
    return emission_after > 0;
  }
  TestGenerator prepare(std::vector<test_event_entry>& events) const;
  std::size_t emission_before = 0;
  std::size_t emission_after = 0;
};

struct EmissionTestRestore : public EmissionTest {
  bool check_block_verification_context(const CryptoNote::block_verification_context& bvc, std::size_t eventIdx,
                                        const CryptoNote::Block& /*blk*/) {
    if (emission_after == 0 || emission_before == 0) {
      return true;
    }
    return emission_after == emission_before + CryptoNote::START_BLOCK_REWARD * 3 - m_currency.calculateInterest(m_currency.depositMinAmount(), m_currency.depositMinTerm());
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

struct TransactionWithDepositExtendsTotalDeposit : public DepositIndexTest {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithMultipleDepositOutsExtendsTotalDeposit : public DepositIndexTest {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithDepositIsClearedAfterInputSpend : public DepositIndexTest {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithDepositUpdatesInterestAfterDepositUnlock : public DepositIndexTest {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithDepositUnrolesInterestAfterSwitchToAlternativeChain : public DepositIndexTest{
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithDepositUnrolesAmountAfterSwitchToAlternativeChain : public DepositIndexTest {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithDepositUpdatesInterestAfterDepositUnlockMultiple : public DepositIndexTest {
  bool generate(std::vector<test_event_entry>& events);
};

struct TransactionWithDepositUnrolesPartOfAmountAfterSwitchToAlternativeChain : public DepositIndexTest {
  bool generate(std::vector<test_event_entry>& events); 
};

}
