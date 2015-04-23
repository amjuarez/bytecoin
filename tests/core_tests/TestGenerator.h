// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once 

#include "chaingen.h"

#include "cryptonote_core/Currency.h"
#include "TransactionBuilder.h"

class TestGenerator {
public:
  TestGenerator(const cryptonote::Currency& currency, std::vector<test_event_entry>& eventsRef) :
      generator(currency),
      events(eventsRef) {
    minerAccount.generate();
    generator.constructBlock(genesisBlock, minerAccount, 1338224400);
    events.push_back(genesisBlock);
    lastBlock = genesisBlock;
  }

  const cryptonote::Currency& currency() const { return generator.currency(); }

  void makeNextBlock(const std::list<cryptonote::Transaction>& txs = std::list<cryptonote::Transaction>()) {
    cryptonote::Block block;
    generator.constructBlock(block, lastBlock, minerAccount, txs);
    events.push_back(block);
    lastBlock = block;
  }

  void makeNextBlock(const cryptonote::Transaction& tx) {
    std::list<cryptonote::Transaction> txs;
    txs.push_back(tx);
    makeNextBlock(txs);
  }

  void generateBlocks() {
    generateBlocks(currency().minedMoneyUnlockWindow());
  }

  void generateBlocks(size_t count, uint8_t majorVersion = cryptonote::BLOCK_MAJOR_VERSION_1) {
    while (count--) {
      cryptonote::Block next;
      generator.constructBlockManually(next, lastBlock, minerAccount, test_generator::bf_major_ver, majorVersion);
      lastBlock = next;
      events.push_back(next);
    }
  }

  TransactionBuilder createTxBuilder(const cryptonote::account_base& from, const cryptonote::account_base& to, uint64_t amount, uint64_t fee) {

    std::vector<cryptonote::tx_source_entry> sources;
    std::vector<cryptonote::tx_destination_entry> destinations;

    fillTxSourcesAndDestinations(sources, destinations, from, to, amount, fee);

    TransactionBuilder builder(generator.currency());

    builder.setInput(sources, from.get_keys());
    builder.setOutput(destinations);

    return builder;
  }

  void fillTxSourcesAndDestinations(
    std::vector<cryptonote::tx_source_entry>& sources, 
    std::vector<cryptonote::tx_destination_entry>& destinations,
    const cryptonote::account_base& from, const cryptonote::account_base& to, uint64_t amount, uint64_t fee, size_t nmix = 0) {
    fill_tx_sources_and_destinations(events, lastBlock, from, to, amount, fee, nmix, sources, destinations);
  }

  void constructTxToKey(
    cryptonote::Transaction& tx,
    const cryptonote::account_base& from,
    const cryptonote::account_base& to,
    uint64_t amount,
    uint64_t fee,
    size_t nmix = 0) {
    construct_tx_to_key(events, tx, lastBlock, from, to, amount, fee, nmix);
  }

  void addEvent(const test_event_entry& e) {
    events.push_back(e);
  }

  void addCallback(const std::string& name) {
    callback_entry cb;
    cb.callback_name = name;
    events.push_back(cb);
  }

  void addCheckAccepted() {
    addCallback("check_block_accepted");
  }

  void addCheckPurged() {
    addCallback("check_block_purged");
  }

  test_generator generator;
  cryptonote::Block genesisBlock;
  cryptonote::Block lastBlock;
  cryptonote::account_base minerAccount;
  std::vector<test_event_entry>& events;
};
