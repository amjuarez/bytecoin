// Copyright (c) 2012-2014, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#include "include_base_utils.h"
#include "cryptonote_core/cryptonote_basic_impl.h"
#include "cryptonote_core/account.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/Currency.h"
#include "misc_language.h"

using namespace cryptonote;

bool test_transaction_generation_and_ring_signature()
{
  cryptonote::Currency currency = cryptonote::CurrencyBuilder().currency();

  account_base miner_acc1;
  miner_acc1.generate();
  account_base miner_acc2;
  miner_acc2.generate();
  account_base miner_acc3;
  miner_acc3.generate();
  account_base miner_acc4;
  miner_acc4.generate();
  account_base miner_acc5;
  miner_acc5.generate();
  account_base miner_acc6;
  miner_acc6.generate();

  std::string add_str = currency.accountAddressAsString(miner_acc3);

  account_base rv_acc;
  rv_acc.generate();
  account_base rv_acc2;
  rv_acc2.generate();
  Transaction tx_mine_1;
  currency.constructMinerTx(0, 0, 0, 10, 0, miner_acc1.get_keys().m_account_address, tx_mine_1);
  Transaction tx_mine_2;
  currency.constructMinerTx(0, 0, 0, 0, 0, miner_acc2.get_keys().m_account_address, tx_mine_2);
  Transaction tx_mine_3;
  currency.constructMinerTx(0, 0, 0, 0, 0, miner_acc3.get_keys().m_account_address, tx_mine_3);
  Transaction tx_mine_4;
  currency.constructMinerTx(0, 0, 0, 0, 0, miner_acc4.get_keys().m_account_address, tx_mine_4);
  Transaction tx_mine_5;
  currency.constructMinerTx(0, 0, 0, 0, 0, miner_acc5.get_keys().m_account_address, tx_mine_5);
  Transaction tx_mine_6;
  currency.constructMinerTx(0, 0, 0, 0, 0, miner_acc6.get_keys().m_account_address, tx_mine_6);

  //fill inputs entry
  typedef tx_source_entry::output_entry tx_output_entry;
  std::vector<tx_source_entry> sources;
  sources.resize(sources.size()+1);
  tx_source_entry& src = sources.back();
  src.amount = 70368744177663;
  {
    tx_output_entry oe;
    oe.first = 0;
    oe.second = boost::get<TransactionOutputToKey>(tx_mine_1.vout[0].target).key;
    src.outputs.push_back(oe);

    oe.first = 1;
    oe.second = boost::get<TransactionOutputToKey>(tx_mine_2.vout[0].target).key;
    src.outputs.push_back(oe);

    oe.first = 2;
    oe.second = boost::get<TransactionOutputToKey>(tx_mine_3.vout[0].target).key;
    src.outputs.push_back(oe);

    oe.first = 3;
    oe.second = boost::get<TransactionOutputToKey>(tx_mine_4.vout[0].target).key;
    src.outputs.push_back(oe);

    oe.first = 4;
    oe.second = boost::get<TransactionOutputToKey>(tx_mine_5.vout[0].target).key;
    src.outputs.push_back(oe);

    oe.first = 5;
    oe.second = boost::get<TransactionOutputToKey>(tx_mine_6.vout[0].target).key;
    src.outputs.push_back(oe);

    src.real_out_tx_key = cryptonote::get_tx_pub_key_from_extra(tx_mine_2);
    src.real_output = 1;
    src.real_output_in_tx_index = 0;
  }
  //fill outputs entry
  tx_destination_entry td;
  td.addr = rv_acc.get_keys().m_account_address;
  td.amount = 69368744177663;
  std::vector<tx_destination_entry> destinations;
  destinations.push_back(td);

  Transaction tx_rc1;
  bool r = construct_tx(miner_acc2.get_keys(), sources, destinations, std::vector<uint8_t>(), tx_rc1, 0);
  CHECK_AND_ASSERT_MES(r, false, "failed to construct transaction");

  crypto::hash pref_hash = get_transaction_prefix_hash(tx_rc1);
  std::vector<const crypto::public_key *> output_keys;
  output_keys.push_back(&boost::get<TransactionOutputToKey>(tx_mine_1.vout[0].target).key);
  output_keys.push_back(&boost::get<TransactionOutputToKey>(tx_mine_2.vout[0].target).key);
  output_keys.push_back(&boost::get<TransactionOutputToKey>(tx_mine_3.vout[0].target).key);
  output_keys.push_back(&boost::get<TransactionOutputToKey>(tx_mine_4.vout[0].target).key);
  output_keys.push_back(&boost::get<TransactionOutputToKey>(tx_mine_5.vout[0].target).key);
  output_keys.push_back(&boost::get<TransactionOutputToKey>(tx_mine_6.vout[0].target).key);
  r = crypto::check_ring_signature(pref_hash, boost::get<TransactionInputToKey>(tx_rc1.vin[0]).keyImage,
    output_keys, &tx_rc1.signatures[0][0]);
  CHECK_AND_ASSERT_MES(r, false, "failed to check ring signature");

  std::vector<size_t> outs;
  uint64_t money = 0;

  r = lookup_acc_outs(rv_acc.get_keys(), tx_rc1, get_tx_pub_key_from_extra(tx_rc1), outs,  money);
  CHECK_AND_ASSERT_MES(r, false, "failed to lookup_acc_outs");
  CHECK_AND_ASSERT_MES(td.amount == money, false, "wrong money amount in new transaction");
  money = 0;
  r = lookup_acc_outs(rv_acc2.get_keys(), tx_rc1, get_tx_pub_key_from_extra(tx_rc1), outs,  money);
  CHECK_AND_ASSERT_MES(r, false, "failed to lookup_acc_outs");
  CHECK_AND_ASSERT_MES(0 == money, false, "wrong money amount in new transaction");
  return true;
}

bool test_block_creation()
{
  uint64_t vszs[] = {80,476,476,475,475,474,475,474,474,475,472,476,476,475,475,474,475,474,474,475,472,476,476,475,475,474,475,474,474,475,9391,476,476,475,475,474,475,8819,8301,475,472,4302,5316,14347,16620,19583,19403,19728,19442,19852,19015,19000,19016,19795,19749,18087,19787,19704,19750,19267,19006,19050,19445,19407,19522,19546,19788,19369,19486,19329,19370,18853,19600,19110,19320,19746,19474,19474,19743,19494,19755,19715,19769,19620,19368,19839,19532,23424,28287,30707};
  std::vector<uint64_t> szs(&vszs[0], &vszs[90]);
  cryptonote::Currency currency = cryptonote::CurrencyBuilder().currency();

  AccountPublicAddress adr;
  bool r = currency.parseAccountAddressString("272xWzbWsP4cfNFfxY5ETN5moU8x81PKfWPwynrrqsNGDBQGLmD1kCkKCvPeDUXu5XfmZkCrQ53wsWmdfvHBGLNjGcRiDcK", adr);
  CHECK_AND_ASSERT_MES(r, false, "failed to import");
  Block b;
  r = currency.constructMinerTx(90, epee::misc_utils::median(szs), 3553616528562147, 33094, 10000000, adr, b.minerTx, blobdata(), 11);
  return r;
}

bool test_transactions()
{
  if(!test_transaction_generation_and_ring_signature())
    return false;
  if(!test_block_creation())
    return false;


  return true;
}
