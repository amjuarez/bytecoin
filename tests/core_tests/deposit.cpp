// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "deposit.h"

#include <cryptonote_config.h>
#include <cryptonote_core/TransactionApi.h>
#include <ITransaction.h>
#include "TransactionBuilder.h"
#include <cstring>

namespace DepositTests {

using namespace cryptonote;
using namespace CryptoNote;

bool DepositTestsBase::check_emission(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events) {
  emission = c.getCoinsInCirculation();
  return true;
}

bool DepositTestsBase::mark_invalid_tx(cryptonote::core& /*c*/, std::size_t ev_index, const std::vector<test_event_entry>& /*events*/) {
  blockId = ev_index + 1;
  return true;
}

TransactionBuilder::MultisignatureSource DepositTestsBase::createSource(uint32_t term, cryptonote::KeyPair key) const {
  TransactionBuilder::MultisignatureSource src;

  src.input.amount = m_currency.depositMinAmount();
  src.input.outputIndex = 0;
  src.input.signatures = 1;
  src.input.term = term;

  src.keys.push_back(from.get_keys());
  src.srcTxPubKey = key.pub;
  src.srcOutputIndex = 0;

  return src;
}

bool DepositTestsBase::check_tx_verification_context(const cryptonote::tx_verification_context& tvc, bool tx_added, std::size_t event_idx, const cryptonote::Transaction& /*tx*/) {
  if (blockId == event_idx)
    return tvc.m_verifivation_failed;
  else
    return !tvc.m_verifivation_failed && tx_added;
}

void DepositTestsBase::addDepositOutput(cryptonote::Transaction& transaction) {
  auto lastOp = transaction.vout.back();
  auto out = boost::get<TransactionOutputToKey>(lastOp.target);
  transaction.vout.pop_back();
  transaction.vout.push_back({m_currency.depositMinAmount(), TransactionOutputMultisignature{{out.key}, 1, m_currency.depositMinTerm() + 1}});
}

Transaction DepositTestsBase::createDepositTransaction(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  auto builder = generator.createTxBuilder(generator.minerAccount, from, 100, m_currency.minimumFee() + 1);
  //builder.
  builder.m_destinations.clear();

  TransactionBuilder::KeysVector kv;
  kv.push_back(to.get_keys());
  
  builder.addMultisignatureOut(1000, kv, 1);
  auto tx = builder.build();
  generator.addEvent(tx);
  return tx;
}

void DepositTestsBase::addDepositInput(cryptonote::Transaction& transaction) {
  auto lastOp = transaction.vin.back();
  auto in = boost::get<TransactionInputToKey>(lastOp);
  transaction.vin.pop_back();
  transaction.vin.push_back(TransactionInputMultisignature{m_currency.depositMinAmount(), 1, 1, m_currency.depositMinTerm() + 1});
/*  transaction.signatures.clear();

  auto trans = CryptoNote::createTransaction(transaction);
  for (auto i = 0; i < transaction.vin.size(); ++i) {
    trans->signInputMultisignature(
        i, reinterpret_cast<const PublicKey&>(to.get_keys().m_account_address.m_viewPublicKey), 1,
        reinterpret_cast<const AccountKeys&>(to.get_keys()));
  }
 */ 
}

bool BlocksOfFirstTypeCantHaveTransactionsOfTypeTwo::generate(std::vector<test_event_entry>& events) {
  MAKE_GENESIS_BLOCK(events, firstBlock, from, 1338224400);
  generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  REWIND_BLOCKS_N(events, otherBlock, firstBlock, from, m_currency.timestampCheckWindow() - 1);
  construct_tx_to_key(events, transaction, firstBlock, from, to, 100, 10, 0);
  addDepositOutput(transaction);
  transaction.version = TRANSACTION_VERSION_2;
  Block secondBlock;
  generator.constructBlockManually(secondBlock, firstBlock, from, test_generator::bf_major_ver, BLOCK_MAJOR_VERSION_1);
  DO_CALLBACK(events, "mark_invalid_block");
  events.push_back(secondBlock);
  return true;
}

bool BlocksOfSecondTypeCanHaveTransactionsOfTypeOne::generate(std::vector<test_event_entry>& events) {
  MAKE_GENESIS_BLOCK(events, firstBlock, from, 1338224400);
  generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  REWIND_BLOCKS_N(events, otherBlock, firstBlock, from, m_currency.timestampCheckWindow() - 1);
  if (!construct_tx_to_key(events, transaction, firstBlock, from, to, 100, 100000, 0)) {
    return false;
  }
  Block secondBlock;
  generator.constructBlock(secondBlock, otherBlock, from, {transaction});
  events.push_back(transaction);
  events.push_back(secondBlock);
  return true;
}

bool BlocksOfSecondTypeCanHaveTransactionsOfTypeTwo::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  auto builder = generator.createTxBuilder(generator.minerAccount, from, 100, m_currency.minimumFee() + 1);
  builder.m_destinations.clear();

  TransactionBuilder::KeysVector kv;
  kv.push_back(to.get_keys());
  
  builder.addMultisignatureOut(m_currency.depositMinAmount() + 1, kv, 1, m_currency.depositMinTerm() + 1);
  auto tx = builder.build();
  generator.addEvent(tx);
  generator.makeNextBlock(tx);
  return true;
}

bool TransactionOfTypeOneWithDepositInputIsRejected::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  cryptonote::KeyPair key;
  {
    auto builder = generator.createTxBuilder(generator.minerAccount, from, 100, m_currency.minimumFee() + 1);
    builder.m_destinations.clear();

    TransactionBuilder::KeysVector kv;
    kv.push_back(to.get_keys());

    builder.addMultisignatureOut(m_currency.depositMinAmount(), kv, 1, m_currency.depositMinTerm() + 1);
    key = builder.getTxKeys();
    auto tx = builder.build();
    generator.addEvent(tx);
    generator.makeNextBlock(tx);
  }
  {
    TransactionBuilder builder(m_currency);
    builder.addMultisignatureInput(createSource(m_currency.depositMinTerm(), key));
    builder.setVersion(TRANSACTION_VERSION_1);
    auto tx = builder.build();
    generator.addCallback("mark_invalid_tx"); // should be rejected by the core
    generator.addEvent(tx);
  }
  return true;
}

bool TransactionOfTypeOneWithDepositOutputIsRejected::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  auto builder = generator.createTxBuilder(generator.minerAccount, from, 100, m_currency.minimumFee() + 1);
  builder.m_destinations.clear();

  TransactionBuilder::KeysVector kv;
  kv.push_back(to.get_keys());
  
  builder.addMultisignatureOut(1000, kv, 1, m_currency.depositMinTerm() + 1);
  builder.setVersion(TRANSACTION_VERSION_1);
  auto tx = builder.build();
  generator.addCallback("mark_invalid_tx"); // should be rejected by the core
  generator.addEvent(tx);
  return true;
}

bool TransactionWithAmountLowerThenMinIsRejected::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  auto builder = generator.createTxBuilder(generator.minerAccount, from, 100, m_currency.minimumFee() + 1);
  builder.m_destinations.clear();

  TransactionBuilder::KeysVector kv;
  kv.push_back(to.get_keys());

  builder.addMultisignatureOut(m_currency.depositMinAmount() - 1, kv, 1, m_currency.depositMinTerm() + 1);
  auto tx = builder.build();
  generator.addCallback("mark_invalid_tx"); // should be rejected by the core
  generator.addEvent(tx);
  return true;
}

bool TransactionWithMinAmountIsAccepted::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  auto builder = generator.createTxBuilder(generator.minerAccount, from, 100, m_currency.minimumFee() + 1);
  builder.m_destinations.clear();

  TransactionBuilder::KeysVector kv;
  kv.push_back(to.get_keys());

  builder.addMultisignatureOut(m_currency.depositMinAmount(), kv, 1, m_currency.depositMinTerm() + 1);
  auto tx = builder.build();
  generator.addEvent(tx);
  return true;
}

bool TransactionWithTermLowerThenMinIsRejected::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  auto builder = generator.createTxBuilder(generator.minerAccount, from, 100, m_currency.minimumFee() + 1);
  builder.m_destinations.clear();

  TransactionBuilder::KeysVector kv;
  kv.push_back(to.get_keys());

  builder.addMultisignatureOut(m_currency.depositMinAmount(), kv, 1, m_currency.depositMinTerm() - 1);
  auto tx = builder.build();
  generator.addCallback("mark_invalid_tx"); // should be rejected by the core
  generator.addEvent(tx);
  return true;
}

bool TransactionWithMinTermIsAccepted::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  auto builder = generator.createTxBuilder(generator.minerAccount, from, 100, m_currency.minimumFee() + 1);
  builder.m_destinations.clear();

  TransactionBuilder::KeysVector kv;
  kv.push_back(to.get_keys());

  builder.addMultisignatureOut(m_currency.depositMinAmount(), kv, 1, m_currency.depositMinTerm());
  auto tx = builder.build();
  generator.addEvent(tx);
  return true;
}

bool TransactionWithTermGreaterThenMaxIsRejected::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  auto builder = generator.createTxBuilder(generator.minerAccount, from, 100, m_currency.minimumFee() + 1);
  builder.m_destinations.clear();

  TransactionBuilder::KeysVector kv;
  kv.push_back(to.get_keys());

  builder.addMultisignatureOut(m_currency.depositMinAmount(), kv, 1, m_currency.depositMaxTerm() + 1);
  auto tx = builder.build();
  generator.addCallback("mark_invalid_tx"); // should be rejected by the core
  generator.addEvent(tx);
  return true;
}

bool TransactionWithMaxTermIsAccepted::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  auto builder = generator.createTxBuilder(generator.minerAccount, from, 100, m_currency.minimumFee() + 1);
  builder.m_destinations.clear();

  TransactionBuilder::KeysVector kv;
  kv.push_back(to.get_keys());

  builder.addMultisignatureOut(m_currency.depositMinAmount(), kv, 1, m_currency.depositMaxTerm());
  auto tx = builder.build();
  generator.addEvent(tx);
  return true;
}

bool TransactionWithoutSignaturesIsRejected::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  auto builder = generator.createTxBuilder(generator.minerAccount, from, 100, m_currency.minimumFee() + 1);
  builder.m_destinations.clear();

  TransactionBuilder::KeysVector kv;
  kv.push_back(to.get_keys());

  builder.addMultisignatureOut(m_currency.depositMinAmount(), kv, 1, m_currency.depositMaxTerm());
  auto tx = builder.build();
  tx.signatures.clear();
  generator.addCallback("mark_invalid_tx"); // should be rejected by the core
  generator.addEvent(tx);
  return true;
}

bool TransactionWithZeroRequiredSignaturesIsRejected::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  auto builder = generator.createTxBuilder(generator.minerAccount, from, 100, m_currency.minimumFee() + 1);
  builder.m_destinations.clear();

  TransactionBuilder::KeysVector kv;
  kv.push_back(to.get_keys());

  builder.addMultisignatureOut(m_currency.depositMinAmount(), kv, 0, m_currency.depositMaxTerm());
  auto tx = builder.build();
  generator.addEvent(tx);
  return true;
}

bool TransactionWithNumberOfRequiredSignaturesGreaterThanKeysIsRejected::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  auto builder = generator.createTxBuilder(generator.minerAccount, from, 100, m_currency.minimumFee() + 1);
  builder.m_destinations.clear();

  TransactionBuilder::KeysVector kv;
  kv.push_back(to.get_keys());

  builder.addMultisignatureOut(m_currency.depositMinAmount(), kv, 2, m_currency.depositMaxTerm());
  auto tx = builder.build();
  generator.addCallback("mark_invalid_tx"); // should be rejected by the core
  generator.addEvent(tx);
  return true;
}

crypto::public_key generate_invalid_pub_key() {
  for (int i = 0; i <= 0xFF; ++i) {
    crypto::public_key key;
    memset(&key, i, sizeof(crypto::public_key));
    if (!crypto::check_key(key)) {
      return key;
    }
  }

  throw std::runtime_error("invalid public key wasn't found");
  return crypto::public_key();
}

bool TransactionWithInvalidKeyIsRejected::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  auto builder = generator.createTxBuilder(generator.minerAccount, from, 100, m_currency.minimumFee() + 1);
  builder.m_destinations.clear();

  auto pub = generate_invalid_pub_key();
  cryptonote::KeyPair k;
  k.pub = pub;
  auto src = createSource(m_currency.depositMinTerm(), k);

  builder.addMultisignatureInput(src);
  auto tx = builder.build();
  generator.addCallback("mark_invalid_tx"); // should be rejected by the core
  generator.addEvent(tx);
  return true;
}

bool TransactionWithDepositExtendsEmission::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  cryptonote::KeyPair key;
  {
    auto builder = generator.createTxBuilder(generator.minerAccount, from, m_currency.depositMinAmount() + 100, m_currency.minimumFee());
    builder.m_destinations.clear();

    TransactionBuilder::KeysVector kv;
    kv.push_back(from.get_keys());

    builder.addMultisignatureOut(m_currency.depositMinAmount(), kv, 1, m_currency.depositMinTerm());
    key = builder.getTxKeys();
    auto tx = builder.build();
    assert(tx.vout.size() == 1);
    generator.addEvent(tx);
    generator.makeNextBlock(tx);
  }
  
  generator.generateBlocks(m_currency.depositMinTerm() - 1, BLOCK_MAJOR_VERSION_2);
  
  {
    TransactionBuilder builder(m_currency);
    auto src = createSource(m_currency.depositMinTerm(), key);
    src.input.amount = m_currency.depositMinAmount();
    builder.addMultisignatureInput(src);
    auto tx = builder.build();
    generator.addEvent(tx);
    generator.addCallback("save_emission_before");
    generator.makeNextBlock(tx);
    generator.addCallback("save_emission_after");
    generator.generateBlocks(1, BLOCK_MAJOR_VERSION_2);
  }
  
  return true;
}

bool TransactionWithDepositRestorsEmissionOnAlternativeChain::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  cryptonote::KeyPair key;
  {
    auto builder = generator.createTxBuilder(generator.minerAccount, from, m_currency.depositMinAmount() + 100, m_currency.minimumFee());
    builder.m_destinations.clear();

    TransactionBuilder::KeysVector kv;
    kv.push_back(from.get_keys());

    builder.addMultisignatureOut(m_currency.depositMinAmount(), kv, 1, m_currency.depositMinTerm());
    key = builder.getTxKeys();
    auto tx = builder.build();
    assert(tx.vout.size() == 1);
    generator.addEvent(tx);
    generator.makeNextBlock(tx);
  }
  
  generator.generateBlocks(m_currency.depositMinTerm() - 1, BLOCK_MAJOR_VERSION_2);
  auto lastBlock = generator.lastBlock;
  
  {
    TransactionBuilder builder(m_currency);
    auto src = createSource(m_currency.depositMinTerm(), key);
    src.input.amount = m_currency.depositMinAmount();
    builder.addMultisignatureInput(src);
    auto tx = builder.build();
    generator.addEvent(tx);
    generator.makeNextBlock(tx);
    generator.addCallback("save_emission_before");
    generator.generateBlocks(1, BLOCK_MAJOR_VERSION_2);
  }
  
  // move to alternative chain 
  generator.lastBlock = lastBlock; 
  generator.generateBlocks(4, BLOCK_MAJOR_VERSION_2);
  generator.addCallback("save_emission_after");
  generator.generateBlocks(1, BLOCK_MAJOR_VERSION_2);
  
  return true;
}

bool TransactionWithOutputToSpentInputWillBeRejected::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  cryptonote::KeyPair key;
  {
    auto builder = generator.createTxBuilder(generator.minerAccount, from, m_currency.depositMinAmount() + 100, m_currency.minimumFee());
    builder.m_destinations.clear();

    TransactionBuilder::KeysVector kv;
    kv.push_back(from.get_keys());

    builder.addMultisignatureOut(m_currency.depositMinAmount(), kv, 1, m_currency.depositMinTerm());
    key = builder.getTxKeys();
    auto tx = builder.build();
    assert(tx.vout.size() == 1);
    generator.addEvent(tx);
    generator.makeNextBlock(tx);
  }
  
  generator.generateBlocks(m_currency.depositMinTerm() - 1, BLOCK_MAJOR_VERSION_2);
  
  {
    TransactionBuilder builder(m_currency);
    auto src = createSource(m_currency.depositMinTerm(), key);
    src.input.amount = m_currency.depositMinAmount();
    builder.addMultisignatureInput(src);
    auto tx = builder.build();
    generator.addEvent(tx);
    generator.makeNextBlock(tx);
  }
  
  {
    TransactionBuilder builder(m_currency);
    auto src = createSource(m_currency.depositMinTerm(), key);
    src.input.amount = m_currency.depositMinAmount();
    builder.addMultisignatureInput(src);
    auto tx = builder.build();
    generator.addCallback("mark_invalid_tx");
    generator.addEvent(tx);
  }
  
  return true;
}

bool TransactionWithMultipleInputsThatSpendOneOutputWillBeRejected::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  cryptonote::KeyPair key;
  {
    auto builder = generator.createTxBuilder(generator.minerAccount, from, m_currency.depositMinAmount() + 100, m_currency.minimumFee());
    builder.m_destinations.clear();

    TransactionBuilder::KeysVector kv;
    kv.push_back(from.get_keys());

    builder.addMultisignatureOut(m_currency.depositMinAmount(), kv, 1, m_currency.depositMinTerm());
    key = builder.getTxKeys();
    auto tx = builder.build();
    assert(tx.vout.size() == 1);
    generator.addEvent(tx);
    generator.makeNextBlock(tx);
  }
  
  generator.generateBlocks(m_currency.depositMinTerm() - 1, BLOCK_MAJOR_VERSION_2);
  
  {
    TransactionBuilder builder(m_currency);
    auto src = createSource(m_currency.depositMinTerm(), key);
    src.input.amount = m_currency.depositMinAmount();
    builder.addMultisignatureInput(src);
    builder.addMultisignatureInput(src);
    auto tx = builder.build();
    generator.addCallback("mark_invalid_tx");
    generator.addEvent(tx);
  }
  
  return true;
}

bool TransactionWithInputWithAmountThatIsDoesntHaveOutputWithSameAmountWillBeRejected::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  cryptonote::KeyPair key;
  {
    auto builder = generator.createTxBuilder(generator.minerAccount, from, m_currency.depositMinAmount() + 100, m_currency.minimumFee());
    builder.m_destinations.clear();

    TransactionBuilder::KeysVector kv;
    kv.push_back(from.get_keys());

    builder.addMultisignatureOut(m_currency.depositMinAmount() + 42, kv, 1, m_currency.depositMinTerm());
    key = builder.getTxKeys();
    auto tx = builder.build();
    assert(tx.vout.size() == 1);
    generator.addEvent(tx);
    generator.makeNextBlock(tx);
  }
  
  generator.generateBlocks(m_currency.depositMinTerm() - 1, BLOCK_MAJOR_VERSION_2);
  
  {
    TransactionBuilder builder(m_currency);
    auto src = createSource(m_currency.depositMinTerm(), key);
    src.input.amount = m_currency.depositMinAmount();
    builder.addMultisignatureInput(src);
    auto tx = builder.build();
    generator.addCallback("mark_invalid_tx");
    generator.addEvent(tx);
  }
  
  return true;
}

//TODO: check
bool TransactionWithInputWithIndexLargerThanNumberOfOutputsWithThisSumWillBeRejected::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  cryptonote::KeyPair key;
  {
    auto builder = generator.createTxBuilder(generator.minerAccount, from, m_currency.depositMinAmount() + 100, m_currency.minimumFee());
    builder.m_destinations.clear();

    TransactionBuilder::KeysVector kv;
    kv.push_back(from.get_keys());

    builder.addMultisignatureOut(m_currency.depositMinAmount(), kv, 1, m_currency.depositMinTerm());
    builder.addMultisignatureOut(m_currency.depositMinAmount(), kv, 1, m_currency.depositMinTerm());
    key = builder.getTxKeys();
    auto tx = builder.build();
    generator.addEvent(tx);
    generator.makeNextBlock(tx);
  }
  
  generator.generateBlocks(m_currency.depositMinTerm() - 1, BLOCK_MAJOR_VERSION_2);
  
  {
    TransactionBuilder builder(m_currency);
    auto src = createSource(m_currency.depositMinTerm(), key);
    src.input.amount = m_currency.depositMinAmount() * 2;
    src.input.outputIndex = 2;
    builder.addMultisignatureInput(src);
    auto tx = builder.build();
    generator.addCallback("mark_invalid_tx");
    generator.addEvent(tx);
  }
  
  return true;
}

bool TransactionWithInputThatPointsToTheOutputButHasAnotherTermWillBeRejected::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  cryptonote::KeyPair key;
  {
    auto builder = generator.createTxBuilder(generator.minerAccount, from, m_currency.depositMinAmount() + 100, m_currency.minimumFee());
    builder.m_destinations.clear();

    TransactionBuilder::KeysVector kv;
    kv.push_back(from.get_keys());

    builder.addMultisignatureOut(m_currency.depositMinAmount(), kv, 1, m_currency.depositMinTerm());
    key = builder.getTxKeys();
    auto tx = builder.build();
    generator.addEvent(tx);
    generator.makeNextBlock(tx);
  }
  
  generator.generateBlocks(m_currency.depositMinTerm() - 1, BLOCK_MAJOR_VERSION_2);
  
  {
    TransactionBuilder builder(m_currency);
    auto src = createSource(m_currency.depositMinTerm(), key);
    src.input.term = m_currency.depositMinTerm() + 1;
    builder.addMultisignatureInput(src);
    auto tx = builder.build();
    generator.addCallback("mark_invalid_tx");
    generator.addEvent(tx);
  }
  
  return true;
}

bool TransactionThatTriesToSpendOutputWhosTermHasntFinishedWillBeRejected::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  cryptonote::KeyPair key;
  {
    auto builder = generator.createTxBuilder(generator.minerAccount, from, m_currency.depositMinAmount() + 100, m_currency.minimumFee());
    builder.m_destinations.clear();

    TransactionBuilder::KeysVector kv;
    kv.push_back(from.get_keys());

    builder.addMultisignatureOut(m_currency.depositMinAmount(), kv, 1, m_currency.depositMinTerm());
    key = builder.getTxKeys();
    auto tx = builder.build();
    generator.addEvent(tx);
    generator.makeNextBlock(tx);
  }
  
  generator.generateBlocks(m_currency.depositMinTerm() - 2, BLOCK_MAJOR_VERSION_2);
  
  {
    TransactionBuilder builder(m_currency);
    auto src = createSource(m_currency.depositMinTerm(), key);
    builder.addMultisignatureInput(src);
    auto tx = builder.build();
    generator.addCallback("mark_invalid_tx");
    generator.addEvent(tx);
  }
  
  return true;
}

bool TransactionWithAmountThatHasAlreadyFinishedWillBeAccepted::generate(std::vector<test_event_entry>& events) {
  TestGenerator generator(m_currency, events);
  generator.generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.generateBlocks(m_currency.minedMoneyUnlockWindow(), BLOCK_MAJOR_VERSION_2);
  cryptonote::KeyPair key;
  {
    auto builder = generator.createTxBuilder(generator.minerAccount, from, m_currency.depositMinAmount() + 100, m_currency.minimumFee());
    builder.m_destinations.clear();

    TransactionBuilder::KeysVector kv;
    kv.push_back(from.get_keys());

    builder.addMultisignatureOut(m_currency.depositMinAmount(), kv, 1, m_currency.depositMinTerm());
    key = builder.getTxKeys();
    auto tx = builder.build();
    generator.addEvent(tx);
    generator.makeNextBlock(tx);
  }
  
  generator.generateBlocks(m_currency.depositMinTerm() - 1, BLOCK_MAJOR_VERSION_2);
  
  {
    TransactionBuilder builder(m_currency);
    auto src = createSource(m_currency.depositMinTerm(), key);
    builder.addMultisignatureInput(src);
    auto tx = builder.build();
    generator.addEvent(tx);
    generator.makeNextBlock(tx);
  }
  
  return true;
}


}
