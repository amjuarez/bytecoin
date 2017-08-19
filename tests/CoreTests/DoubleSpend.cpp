// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "DoubleSpend.h"
#include "TestGenerator.h"

using namespace CryptoNote;

//======================================================================================================================

gen_double_spend_in_different_chains::gen_double_spend_in_different_chains()
{
  expected_blockchain_height = 4 + 2 * m_currency.minedMoneyUnlockWindow();

  REGISTER_CALLBACK_METHOD(gen_double_spend_in_different_chains, check_double_spend);
}

bool gen_double_spend_in_different_chains::generate(std::vector<test_event_entry>& events) const
{
  INIT_DOUBLE_SPEND_TEST();

  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, true);
  MAKE_TX(events, tx_1, bob_account, alice_account, send_amount / 2 - m_currency.minimumFee(), blk_1);
  events.pop_back();
  MAKE_TX(events, tx_2, bob_account, alice_account, send_amount - m_currency.minimumFee(), blk_1);
  events.pop_back();

  // Main chain
  events.push_back(tx_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1r, miner_account, tx_1);

  // Alternative chain
  events.push_back(tx_2);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_1r, miner_account, tx_2);
  // Switch to alternative chain
  MAKE_NEXT_BLOCK(events, blk_4, blk_3, miner_account);
  CHECK_AND_NO_ASSERT_MES(expected_blockchain_height == get_block_height(blk_4) + 1, false, "expected_blockchain_height has invalid value");

  DO_CALLBACK(events, "check_double_spend");

  return true;
}

bool gen_double_spend_in_different_chains::check_double_spend(CryptoNote::core& c, size_t /*ev_index*/, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_double_spend_in_different_chains::check_double_spend");

  std::list<Block> block_list;
  bool r = c.get_blocks(0, 100 + 2 * m_currency.minedMoneyUnlockWindow(), block_list);
  CHECK_TEST_CONDITION(r);

  std::vector<Block> blocks(block_list.begin(), block_list.end());
  CHECK_EQ(expected_blockchain_height, blocks.size());

  CHECK_EQ(1, c.get_pool_transactions_count());
  CHECK_EQ(1, c.get_alternative_blocks_count());

  CryptoNote::AccountBase bob_account = boost::get<CryptoNote::AccountBase>(events[1]);
  CryptoNote::AccountBase alice_account = boost::get<CryptoNote::AccountBase>(events[2]);

  std::vector<CryptoNote::Block> chain;
  map_hash2tx_t mtx;
  r = find_block_chain(events, chain, mtx, get_block_hash(blocks.back()));
  CHECK_TEST_CONDITION(r);
  CHECK_EQ(0, get_balance(bob_account, blocks, mtx));
  CHECK_EQ(send_amount - m_currency.minimumFee(), get_balance(alice_account, blocks, mtx));

  return true;
}

//======================================================================================================================
// DoubleSpendBase
//======================================================================================================================
DoubleSpendBase::DoubleSpendBase() :
  m_invalid_tx_index(invalid_index_value),
  m_invalid_block_index(invalid_index_value),
  send_amount(MK_COINS(17)),
  has_invalid_tx(false)
{
  m_outputTxKey = generateKeyPair();
  m_bob_account.generate();
  m_alice_account.generate();

  REGISTER_CALLBACK_METHOD(DoubleSpendBase, mark_last_valid_block);
  REGISTER_CALLBACK_METHOD(DoubleSpendBase, mark_invalid_tx);
  REGISTER_CALLBACK_METHOD(DoubleSpendBase, mark_invalid_block);
  REGISTER_CALLBACK_METHOD(DoubleSpendBase, check_double_spend);
}

bool DoubleSpendBase::check_tx_verification_context(const CryptoNote::tx_verification_context& tvc, bool tx_added, size_t event_idx, const CryptoNote::Transaction& /*tx*/)
{
  if (m_invalid_tx_index == event_idx)
    return tvc.m_verifivation_failed;
  else
    return !tvc.m_verifivation_failed && tx_added;
}

bool DoubleSpendBase::check_block_verification_context(const CryptoNote::block_verification_context& bvc, size_t event_idx, const CryptoNote::Block& /*block*/)
{
  if (m_invalid_block_index == event_idx)
    return bvc.m_verifivation_failed;
  else
    return !bvc.m_verifivation_failed;
}

bool DoubleSpendBase::mark_last_valid_block(CryptoNote::core& c, size_t /*ev_index*/, const std::vector<test_event_entry>& /*events*/)
{
  m_last_valid_block = c.get_tail_id();
  return true;
}

bool DoubleSpendBase::mark_invalid_tx(CryptoNote::core& /*c*/, size_t ev_index, const std::vector<test_event_entry>& /*events*/)
{
  m_invalid_tx_index = ev_index + 1;
  return true;
}

bool DoubleSpendBase::mark_invalid_block(CryptoNote::core& /*c*/, size_t ev_index, const std::vector<test_event_entry>& /*events*/)
{
  m_invalid_block_index = ev_index + 1;
  return true;
}

bool DoubleSpendBase::check_double_spend(CryptoNote::core& c, size_t /*ev_index*/, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("DoubleSpendBase::check_double_spend");
  CHECK_EQ(m_last_valid_block, c.get_tail_id());
  return true;
}

TestGenerator DoubleSpendBase::prepare(std::vector<test_event_entry>& events) const {

  TestGenerator generator(m_currency, events);

  // unlock
  generator.generateBlocks();

  auto builder = generator.createTxBuilder(generator.minerAccount, m_bob_account, send_amount, m_currency.minimumFee());

  builder.setTxKeys(m_outputTxKey);
  builder.m_destinations.clear();

  TransactionBuilder::KeysVector kv;
  kv.push_back(m_bob_account.getAccountKeys());
  
  builder.addMultisignatureOut(send_amount, kv, 1);

  // move money
  auto tx = builder.build();
    
  generator.addEvent(tx);
  generator.makeNextBlock(tx);

  // unlock
  generator.generateBlocks(); 

  return generator;
}


TransactionBuilder::MultisignatureSource DoubleSpendBase::createSource() const {

  TransactionBuilder::MultisignatureSource src;

  src.input.amount = send_amount;
  src.input.outputIndex = 0;
  src.input.signatureCount = 1;

  src.keys.push_back(m_bob_account.getAccountKeys());
  src.srcTxPubKey = m_outputTxKey.publicKey;
  src.srcOutputIndex = 0;

  return src;
}

TransactionBuilder DoubleSpendBase::createBobToAliceTx() const {
  TransactionBuilder builder(m_currency);

  builder.
    addMultisignatureInput(createSource()).
    addOutput(TransactionDestinationEntry(send_amount - m_currency.minimumFee(), m_alice_account.getAccountKeys().address));

  return builder;
}

//======================================================================================================================
// MultiSigTx_DoubleSpendInTx
//======================================================================================================================

MultiSigTx_DoubleSpendInTx::MultiSigTx_DoubleSpendInTx(bool txsKeepedByBlock) 
  : m_txsKeepedByBlock(txsKeepedByBlock)
{
  has_invalid_tx = true;
}

bool MultiSigTx_DoubleSpendInTx::generate(std::vector<test_event_entry>& events) const {
  TestGenerator generator(prepare(events));

  generator.addCallback("mark_last_valid_block");

  TransactionBuilder builder(generator.currency());

  auto tx = builder.
    addMultisignatureInput(createSource()).
    addMultisignatureInput(createSource()).
    addOutput(TransactionDestinationEntry(send_amount*2 - m_currency.minimumFee(), m_alice_account.getAccountKeys().address)).
    build();

  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, m_txsKeepedByBlock);

  generator.addCallback("mark_invalid_tx"); // should be rejected by the core
  generator.addEvent(tx);
  generator.addCallback("mark_invalid_block");
  generator.makeNextBlock(tx);
  generator.addCallback("check_double_spend");

  return true;
}

//======================================================================================================================
// MultiSigTx_DoubleSpendSameBlock
//======================================================================================================================
MultiSigTx_DoubleSpendSameBlock::MultiSigTx_DoubleSpendSameBlock(bool txsKeepedByBlock) 
  : m_txsKeepedByBlock(txsKeepedByBlock) {
  has_invalid_tx = !txsKeepedByBlock;
}

bool MultiSigTx_DoubleSpendSameBlock::generate(std::vector<test_event_entry>& events) const {
  TestGenerator generator(prepare(events));

  generator.addCallback("mark_last_valid_block");
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, m_txsKeepedByBlock);

  std::list<Transaction> txs;

  auto builder = createBobToAliceTx();

  auto tx1 = builder.newTxKeys().build();
  auto tx2 = builder.newTxKeys().build();

  generator.addEvent(tx1);

  if (has_invalid_tx) {
    generator.addCallback("mark_invalid_tx");
  }

  generator.addEvent(tx2);
  
  txs.push_back(tx1);
  txs.push_back(tx2);

  generator.addCallback("mark_invalid_block");
  generator.makeNextBlock(txs);
  generator.addCallback("check_double_spend");

  return true;
}

//======================================================================================================================
// MultiSigTx_DoubleSpendDifferentBlocks
//======================================================================================================================
MultiSigTx_DoubleSpendDifferentBlocks::MultiSigTx_DoubleSpendDifferentBlocks(bool txsKeepedByBlock)
  : m_txsKeepedByBlock(txsKeepedByBlock) { 
  has_invalid_tx = !txsKeepedByBlock;
}

bool MultiSigTx_DoubleSpendDifferentBlocks::generate(std::vector<test_event_entry>& events) const {
  TestGenerator generator(prepare(events));

  generator.addCallback("mark_last_valid_block");
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, m_txsKeepedByBlock);

  auto builder = createBobToAliceTx();

  auto tx1 = builder.build();

  generator.addEvent(tx1);
  generator.makeNextBlock(tx1);
  generator.addCallback("mark_last_valid_block");

  auto tx2 = builder.newTxKeys().build(); // same transaction, but different tx key

  if (has_invalid_tx) {
    generator.addCallback("mark_invalid_tx");
  }

  generator.addEvent(tx2);
  generator.addCallback("mark_invalid_block");
  generator.makeNextBlock(tx2);
  generator.addCallback("check_double_spend");

  return true;
}

//======================================================================================================================
// MultiSigTx_DoubleSpendAltChainSameBlock
//======================================================================================================================

MultiSigTx_DoubleSpendAltChainSameBlock::MultiSigTx_DoubleSpendAltChainSameBlock(bool txsKeepedByBlock)
  : m_txsKeepedByBlock(txsKeepedByBlock) {
  has_invalid_tx = !txsKeepedByBlock;
}

bool MultiSigTx_DoubleSpendAltChainSameBlock::generate(std::vector<test_event_entry>& events) const {
  TestGenerator mainChain(prepare(events));
  TestGenerator altChain(mainChain);

  mainChain.makeNextBlock(); // main chain
  mainChain.addCallback("mark_last_valid_block");

  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, m_txsKeepedByBlock);

  auto builder = createBobToAliceTx();

  std::list<Transaction> txs;
  auto tx1 = builder.build();
  auto tx2 = builder.newTxKeys().build();
  txs.push_back(tx1);
  txs.push_back(tx2);

  altChain.addEvent(tx1);
  altChain.addEvent(tx2);
  altChain.makeNextBlock(txs);
  altChain.generateBlocks(); // force switch to alt chain

  mainChain.addCallback("check_double_spend");
  return true;
}

//======================================================================================================================
// MultiSigTx_DoubleSpendAltChainDifferentBlocks
//======================================================================================================================

MultiSigTx_DoubleSpendAltChainDifferentBlocks::MultiSigTx_DoubleSpendAltChainDifferentBlocks(bool txsKeepedByBlock)
  : m_txsKeepedByBlock(txsKeepedByBlock) {
  has_invalid_tx = !txsKeepedByBlock;
}

bool MultiSigTx_DoubleSpendAltChainDifferentBlocks::generate(std::vector<test_event_entry>& events) const {
  TestGenerator mainChain(prepare(events));
  TestGenerator altChain(mainChain);

  mainChain.makeNextBlock(); // main chain

  mainChain.addCallback("mark_last_valid_block");
  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, m_txsKeepedByBlock);

  auto builder = createBobToAliceTx();

  auto tx1 = builder.build();

  altChain.addEvent(tx1);
  altChain.makeNextBlock(tx1);
  altChain.addCallback("mark_last_valid_block");

  auto tx2 = builder.newTxKeys().build();

  if (has_invalid_tx) {
    altChain.addCallback("mark_invalid_tx");
  }

  altChain.addEvent(tx2);
  altChain.addCallback("mark_invalid_block");
  altChain.makeNextBlock(tx2);

  mainChain.addCallback("check_double_spend");

  return true;
}
