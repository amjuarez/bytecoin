// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ChainSplit1.h"

using namespace std;
using namespace CryptoNote;


gen_simple_chain_split_1::gen_simple_chain_split_1()
{
  REGISTER_CALLBACK("check_split_not_switched", gen_simple_chain_split_1::check_split_not_switched);
  REGISTER_CALLBACK("check_split_not_switched2", gen_simple_chain_split_1::check_split_not_switched2);
  REGISTER_CALLBACK("check_split_switched", gen_simple_chain_split_1::check_split_switched);
  REGISTER_CALLBACK("check_split_not_switched_back", gen_simple_chain_split_1::check_split_not_switched_back);
  REGISTER_CALLBACK("check_split_switched_back_1", gen_simple_chain_split_1::check_split_switched_back_1);
  REGISTER_CALLBACK("check_split_switched_back_2", gen_simple_chain_split_1::check_split_switched_back_2);
  REGISTER_CALLBACK("check_mempool_1", gen_simple_chain_split_1::check_mempool_1);
  REGISTER_CALLBACK("check_mempool_2", gen_simple_chain_split_1::check_mempool_2);
  //REGISTER_CALLBACK("check_orphaned_chain_1", gen_simple_chain_split_1::check_orphaned_chain_1);
  //REGISTER_CALLBACK("check_orphaned_switched_to_alternative", gen_simple_chain_split_1::check_orphaned_switched_to_alternative);
  //REGISTER_CALLBACK("check_orphaned_chain_2", gen_simple_chain_split_1::check_orphaned_chain_2);
  //REGISTER_CALLBACK("check_orphaned_switched_to_main", gen_simple_chain_split_1::check_orphaned_switched_to_main);
  //REGISTER_CALLBACK("check_orphaned_chain_38", gen_simple_chain_split_1::check_orphaned_chain_38);
  //REGISTER_CALLBACK("check_orphaned_chain_39", gen_simple_chain_split_1::check_orphaned_chain_39);
  //REGISTER_CALLBACK("check_orphaned_chain_40", gen_simple_chain_split_1::check_orphaned_chain_40);
  //REGISTER_CALLBACK("check_orphaned_chain_41", gen_simple_chain_split_1::check_orphaned_chain_41);
}
//-----------------------------------------------------------------------------------------------------
bool gen_simple_chain_split_1::generate(std::vector<test_event_entry> &events) const
{
  uint64_t ts_start = 1338224400;
  /*
   1    2    3    4    5    6     7     8      9    10    11    12    13    14    15    16    17    18   19   20     21    22    23   <-- main blockchain height
  (0 )-(1 )-(2 )-(3 )-(4 )-(5 ) -(6 ) -(7 ) -(8 )|-(17) -(18) -(19) -(20) -(21)|-(22)|-(23)|-(24)|
                              \ -(9 ) -(10)|-(11)|-(12)|-(13) -(14) -(15) -(16)       
                                                                                      -(25) -(26)|
                                                                                -(27)|             #check switching to alternative
                                                              ----------------------------------------------------------------------------------
                                                                                      -(28) -(29) -(30) -(31)|
                                                                                -(32)|              #check switching orphans to main
                                                              ----------------------------------------------------------------------------------
                                                                                      -(33) -(34)       -(35) -(36)       -(37) -(38)|
                                                                                -(39)|           #<--this part becomes alternative chain connected to main
                                                                                                                    -(40)|  #still marked as orphaned 
                                                                                                  -(41)|
                                                                                                   #check orphaned with block in the middle of the orphaned chain 
  */

  GENERATE_ACCOUNT(first_miner_account);
  //                                                                                          events index
  MAKE_GENESIS_BLOCK(events, blk_0, first_miner_account, ts_start);                           //  0
  MAKE_NEXT_BLOCK(events, blk_1, blk_0, first_miner_account);                                 //  1
  MAKE_NEXT_BLOCK(events, blk_2, blk_1, first_miner_account);                                 //  2
  MAKE_NEXT_BLOCK(events, blk_3, blk_2, first_miner_account);                                 //  3
  MAKE_NEXT_BLOCK(events, blk_4, blk_3, first_miner_account);                                 //  4
  MAKE_NEXT_BLOCK(events, blk_5, blk_4, first_miner_account);                                 //  5
  MAKE_NEXT_BLOCK(events, blk_6, blk_5, first_miner_account);                                 //  6
  MAKE_NEXT_BLOCK(events, blk_7, blk_6, first_miner_account);                                 //  7
  MAKE_NEXT_BLOCK(events, blk_8, blk_7, first_miner_account);                                 //  8
  //split
  MAKE_NEXT_BLOCK(events, blk_9, blk_5, first_miner_account);                                 //  9
  MAKE_NEXT_BLOCK(events, blk_10, blk_9, first_miner_account);                                //  10
  DO_CALLBACK(events, "check_split_not_switched");                                            //  11
  MAKE_NEXT_BLOCK(events, blk_11, blk_10, first_miner_account);                               //  12
  DO_CALLBACK(events, "check_split_not_switched2");                                           //  13
  MAKE_NEXT_BLOCK(events, blk_12, blk_11, first_miner_account);                               //  14
  DO_CALLBACK(events, "check_split_switched");                                                //  15
  MAKE_NEXT_BLOCK(events, blk_13, blk_12, first_miner_account);                               //  16
  MAKE_NEXT_BLOCK(events, blk_14, blk_13, first_miner_account);                               //  17
  MAKE_NEXT_BLOCK(events, blk_15, blk_14, first_miner_account);                               //  18
  MAKE_NEXT_BLOCK(events, blk_16, blk_15, first_miner_account);                               //  19
  //split again and check back switching
  MAKE_NEXT_BLOCK(events, blk_17, blk_8, first_miner_account);                                //  20
  MAKE_NEXT_BLOCK(events, blk_18, blk_17,  first_miner_account);                              //  21
  MAKE_NEXT_BLOCK(events, blk_19, blk_18,  first_miner_account);                              //  22
  MAKE_NEXT_BLOCK(events, blk_20, blk_19,  first_miner_account);                              //  23
  MAKE_NEXT_BLOCK(events, blk_21, blk_20,  first_miner_account);                              //  24
  DO_CALLBACK(events, "check_split_not_switched_back");                                       //  25
  MAKE_NEXT_BLOCK(events, blk_22, blk_21, first_miner_account);                               //  26
  DO_CALLBACK(events, "check_split_switched_back_1");                                         //  27
  MAKE_NEXT_BLOCK(events, blk_23, blk_22, first_miner_account);                               //  28
  DO_CALLBACK(events, "check_split_switched_back_2");                                         //  29

  REWIND_BLOCKS(events, blk_23r, blk_23, first_miner_account);                                //  30...N1
  GENERATE_ACCOUNT(alice);
  MAKE_TX(events, tx_0, first_miner_account, alice, MK_COINS(10), blk_23);                    //  N1+1
  MAKE_TX(events, tx_1, first_miner_account, alice, MK_COINS(20), blk_23);                    //  N1+2
  MAKE_TX(events, tx_2, first_miner_account, alice, MK_COINS(30), blk_23);                    //  N1+3
  DO_CALLBACK(events, "check_mempool_1");                                                     //  N1+4
  MAKE_NEXT_BLOCK_TX1(events, blk_24, blk_23r, first_miner_account, tx_0);                    //  N1+5
  DO_CALLBACK(events, "check_mempool_2");                                                     //  N1+6
  /*
  //check orphaned blocks
  MAKE_NEXT_BLOCK_NO_ADD(events, blk_orph_27, blk_16, get_test_target(), first_miner_account);     
  MAKE_NEXT_BLOCK(events, blk_25, blk_orph_27, get_test_target(), first_miner_account);       //  36
  MAKE_NEXT_BLOCK(events, blk_26, blk_25, get_test_target(), first_miner_account);            //  37
  DO_CALLBACK(events, "check_orphaned_chain_1");                                              //  38
  ADD_BLOCK(events, blk_orph_27);                                                             //  39
  DO_CALLBACK(events, "check_orphaned_switched_to_alternative");                              //  40
  
  //check orphaned check to main chain
  MAKE_NEXT_BLOCK_NO_ADD(events, blk_orph_32, blk_16, get_test_target(), first_miner_account);     
  MAKE_NEXT_BLOCK(events, blk_28, blk_orph_32, get_test_target(), first_miner_account);       //  41
  MAKE_NEXT_BLOCK(events, blk_29, blk_28, get_test_target(), first_miner_account);            //  42
  MAKE_NEXT_BLOCK(events, blk_30, blk_29, get_test_target(), first_miner_account);            //  43
  MAKE_NEXT_BLOCK(events, blk_31, blk_30, get_test_target(), first_miner_account);            //  44
  DO_CALLBACK(events, "check_orphaned_chain_2");                                              //  45
  ADD_BLOCK(events, blk_orph_32);                                                             //  46
  DO_CALLBACK(events, "check_orphaned_switched_to_main");                                     //  47

  //check orphaned check to main chain
  MAKE_NEXT_BLOCK_NO_ADD(events, blk_orph_39, blk_16, get_test_target(), first_miner_account);     
  MAKE_NEXT_BLOCK(events, blk_33, blk_orph_39, get_test_target(), first_miner_account);       //  48
  MAKE_NEXT_BLOCK(events, blk_34, blk_33, get_test_target(), first_miner_account);            //  49
  MAKE_NEXT_BLOCK_NO_ADD(events, blk_orph_41, blk_34, get_test_target(), first_miner_account);     
  MAKE_NEXT_BLOCK(events, blk_35, blk_orph_41, get_test_target(), first_miner_account);       //  50
  MAKE_NEXT_BLOCK(events, blk_36, blk_35, get_test_target(), first_miner_account);            //  51
  MAKE_NEXT_BLOCK_NO_ADD(events, blk_orph_40, blk_36, get_test_target(), first_miner_account);     
  MAKE_NEXT_BLOCK(events, blk_37, blk_orph_40, get_test_target(), first_miner_account);       //  52
  MAKE_NEXT_BLOCK(events, blk_38, blk_37, get_test_target(), first_miner_account);            //  53
  DO_CALLBACK(events, "check_orphaned_chain_38");                                             //  54
  ADD_BLOCK(events, blk_orph_39);                                                             //  55
  DO_CALLBACK(events, "check_orphaned_chain_39");                                             //  56
  ADD_BLOCK(events, blk_orph_40);                                                             //  57
  DO_CALLBACK(events, "check_orphaned_chain_40");                                             //  58
  ADD_BLOCK(events, blk_orph_41);                                                             //  59
  DO_CALLBACK(events, "check_orphaned_chain_41");                                             //  60
  */
  return true;
}
//-----------------------------------------------------------------------------------------------------
bool gen_simple_chain_split_1::check_mempool_2(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_simple_chain_split_1::check_mempool_2");
  CHECK_TEST_CONDITION(c.get_pool_transactions_count() == 2);
  return true;
}
//-----------------------------------------------------------------------------------------------------
bool gen_simple_chain_split_1::check_mempool_1(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_simple_chain_split_1::check_mempool_1");
  CHECK_TEST_CONDITION(c.get_pool_transactions_count() == 3);
  return true;
}
//-----------------------------------------------------------------------------------------------------
bool gen_simple_chain_split_1::check_split_not_switched(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_simple_chain_split_1::check_split_not_switched");
  //check height
  CHECK_TEST_CONDITION(c.get_current_blockchain_height() == 9);
  CHECK_TEST_CONDITION(c.get_blockchain_total_transactions() == 9);
  CHECK_TEST_CONDITION(c.get_tail_id() == get_block_hash(boost::get<CryptoNote::Block>(events[8])));
  CHECK_TEST_CONDITION(c.get_alternative_blocks_count() == 2);
  return true;
}
//-----------------------------------------------------------------------------------------------------
bool gen_simple_chain_split_1::check_split_not_switched2(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_simple_chain_split_1::check_split_not_switched2");
  //check height
  CHECK_TEST_CONDITION(c.get_current_blockchain_height() == 9);
  CHECK_TEST_CONDITION(c.get_blockchain_total_transactions() == 9);
  CHECK_TEST_CONDITION(c.get_tail_id() == get_block_hash(boost::get<CryptoNote::Block>(events[8])));
  CHECK_TEST_CONDITION(c.get_alternative_blocks_count() == 3);
  return true;
}
//-----------------------------------------------------------------------------------------------------
bool gen_simple_chain_split_1::check_split_switched(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_simple_chain_split_1::check_split_switched");

  //check height
  CHECK_TEST_CONDITION(c.get_current_blockchain_height() == 10);
  CHECK_TEST_CONDITION(c.get_blockchain_total_transactions() == 10);
  CHECK_TEST_CONDITION(c.get_tail_id() == get_block_hash(boost::get<CryptoNote::Block>(events[14])));
  CHECK_TEST_CONDITION(c.get_alternative_blocks_count() == 3);
  return true;
}
//-----------------------------------------------------------------------------------------------------
bool gen_simple_chain_split_1::check_split_not_switched_back(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_simple_chain_split_1::check_split_not_switched_back");
  //check height
  CHECK_TEST_CONDITION(c.get_current_blockchain_height() == 14);
  CHECK_TEST_CONDITION(c.get_blockchain_total_transactions() == 14);
  CHECK_TEST_CONDITION(c.get_tail_id() == get_block_hash(boost::get<CryptoNote::Block>(events[19])));
  CHECK_TEST_CONDITION(c.get_alternative_blocks_count() == 8);

  return true;
}
//-----------------------------------------------------------------------------------------------------
bool gen_simple_chain_split_1::check_split_switched_back_1(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_simple_chain_split_1::check_split_switched_back_1");

  //check height
  CHECK_TEST_CONDITION(c.get_current_blockchain_height()== 15);
  CHECK_TEST_CONDITION(c.get_blockchain_total_transactions() == 15);
  CHECK_TEST_CONDITION(c.get_tail_id() == get_block_hash(boost::get<CryptoNote::Block>(events[26])));
  CHECK_TEST_CONDITION(c.get_alternative_blocks_count() == 8);

  return true;
}//-----------------------------------------------------------------------------------------------------
bool gen_simple_chain_split_1::check_split_switched_back_2(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_simple_chain_split_1::check_split_switched_back_2");

  //check height
  CHECK_TEST_CONDITION(c.get_current_blockchain_height() == 16);
  CHECK_TEST_CONDITION(c.get_blockchain_total_transactions() == 16);
  CHECK_TEST_CONDITION(c.get_tail_id() == get_block_hash(boost::get<CryptoNote::Block>(events[28])));
  CHECK_TEST_CONDITION(c.get_alternative_blocks_count() == 8);
  return true;
}
//-----------------------------------------------------------------------------------------------------
/*
bool gen_simple_chain_split_1::check_orphaned_chain_1(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_simple_chain_split_1::check_orphaned_chain_1");
  CHECK_TEST_CONDITION(c.get_orphaned_by_prev_blocks_count() == 2);
  return true;
}
//-----------------------------------------------------------------------------------------------------
bool gen_simple_chain_split_1::check_orphaned_switched_to_alternative(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_simple_chain_split_1::check_orphaned_switched_to_alternative");
  CHECK_TEST_CONDITION(c.get_orphaned_by_prev_blocks_count() == 0);
  CHECK_TEST_CONDITION(c.get_current_blockchain_height()== 17);
  CHECK_TEST_CONDITION(c.get_alternative_blocks_count() == 11);
  return true;
}
//-----------------------------------------------------------------------------------------------------
bool gen_simple_chain_split_1::check_orphaned_chain_2(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_simple_chain_split_1::check_orphaned_chain_2");
  CHECK_TEST_CONDITION(c.get_orphaned_by_prev_blocks_count() == 4);
  return true;
}
//-----------------------------------------------------------------------------------------------------
bool gen_simple_chain_split_1::check_orphaned_switched_to_main(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_simple_chain_split_1::check_orphaned_switched_to_main");
  CHECK_TEST_CONDITION(c.get_orphaned_by_prev_blocks_count() == 0);
  CHECK_TEST_CONDITION(c.get_current_blockchain_height()== 19);
  CHECK_TEST_CONDITION(c.get_alternative_blocks_count() == 14);
  return true;
}
//-----------------------------------------------------------------------------------------------------
bool gen_simple_chain_split_1::check_orphaned_chain_38(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_simple_chain_split_1::check_orphaned_chain_38");
  CHECK_TEST_CONDITION(c.get_orphaned_by_prev_blocks_count() == 6);
  CHECK_TEST_CONDITION(c.get_alternative_blocks_count() == 14);
  CHECK_TEST_CONDITION(c.get_current_blockchain_height()== 19);
  return true;
}
//-----------------------------------------------------------------------------------------------------
bool gen_simple_chain_split_1::check_orphaned_chain_39(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_simple_chain_split_1::check_orphaned_chain_39");
  CHECK_TEST_CONDITION(c.get_orphaned_by_prev_blocks_count() == 4);
  CHECK_TEST_CONDITION(c.get_alternative_blocks_count() == 17);
  CHECK_TEST_CONDITION(c.get_current_blockchain_height()== 19);
  return true;
}
//-----------------------------------------------------------------------------------------------------
bool gen_simple_chain_split_1::check_orphaned_chain_40(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_simple_chain_split_1::check_orphaned_chain_40");
  CHECK_TEST_CONDITION(c.get_orphaned_by_prev_blocks_count() == 5);
  CHECK_TEST_CONDITION(c.get_alternative_blocks_count() == 17);
  CHECK_TEST_CONDITION(c.get_current_blockchain_height()== 19);
  return true;
}
//-----------------------------------------------------------------------------------------------------
bool gen_simple_chain_split_1::check_orphaned_chain_41(CryptoNote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_simple_chain_split_1::check_orphaned_chain_41");
  CHECK_TEST_CONDITION(c.get_orphaned_by_prev_blocks_count() == 0);
  CHECK_TEST_CONDITION(c.get_alternative_blocks_count() == 19);
  CHECK_TEST_CONDITION(c.get_current_blockchain_height()== 23);

  return true;
}*/
//-----------------------------------------------------------------------------------------------------
