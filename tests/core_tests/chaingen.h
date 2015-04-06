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

#pragma once

#include <boost/program_options.hpp>
#include <boost/serialization/variant.hpp>
#include "cryptonote_core/CoreConfig.h"

#include "common/boost_serialization_helper.h"
#include "common/command_line.h"
#include "cryptonote_core/account_boost_serialization.h"
#include "cryptonote_core/account.h"
#include "cryptonote_core/cryptonote_core.h"

#include "../TestGenerator/TestGenerator.h"

namespace concolor
{
  inline std::basic_ostream<char, std::char_traits<char> >& bright_white(std::basic_ostream<char, std::char_traits<char> >& ostr)
  {
    epee::log_space::set_console_color(epee::log_space::console_color_white, true);
    return ostr;
  }

  inline std::basic_ostream<char, std::char_traits<char> >& red(std::basic_ostream<char, std::char_traits<char> >& ostr)
  {
    epee::log_space::set_console_color(epee::log_space::console_color_red, true);
    return ostr;
  }

  inline std::basic_ostream<char, std::char_traits<char> >& green(std::basic_ostream<char, std::char_traits<char> >& ostr)
  {
    epee::log_space::set_console_color(epee::log_space::console_color_green, true);
    return ostr;
  }

  inline std::basic_ostream<char, std::char_traits<char> >& magenta(std::basic_ostream<char, std::char_traits<char> >& ostr)
  {
    epee::log_space::set_console_color(epee::log_space::console_color_magenta, true);
    return ostr;
  }

  inline std::basic_ostream<char, std::char_traits<char> >& yellow(std::basic_ostream<char, std::char_traits<char> >& ostr)
  {
    epee::log_space::set_console_color(epee::log_space::console_color_yellow, true);
    return ostr;
  }

  inline std::basic_ostream<char, std::char_traits<char> >& normal(std::basic_ostream<char, std::char_traits<char> >& ostr)
  {
    epee::log_space::reset_console_color();
    return ostr;
  }
}


struct callback_entry
{
  std::string callback_name;
  BEGIN_SERIALIZE_OBJECT()
    FIELD(callback_name)
  END_SERIALIZE()

private:
  friend class boost::serialization::access;

  template<class Archive>
  void serialize(Archive & ar, const unsigned int /*version*/)
  {
    ar & callback_name;
  }
};

template<typename T>
struct serialized_object
{
  serialized_object() { }

  serialized_object(const cryptonote::blobdata& a_data)
    : data(a_data)
  {
  }

  cryptonote::blobdata data;
  BEGIN_SERIALIZE_OBJECT()
    FIELD(data)
    END_SERIALIZE()

private:
  friend class boost::serialization::access;

  template<class Archive>
  void serialize(Archive & ar, const unsigned int /*version*/)
  {
    ar & data;
  }
};

typedef serialized_object<cryptonote::Block> serialized_block;
typedef serialized_object<cryptonote::Transaction> serialized_transaction;

struct event_visitor_settings
{
  int valid_mask;
  bool txs_keeped_by_block;

  enum settings
  {
    set_txs_keeped_by_block = 1 << 0
  };

  event_visitor_settings(int a_valid_mask = 0, bool a_txs_keeped_by_block = false)
    : valid_mask(a_valid_mask)
    , txs_keeped_by_block(a_txs_keeped_by_block)
  {
  }

private:
  friend class boost::serialization::access;

  template<class Archive>
  void serialize(Archive & ar, const unsigned int /*version*/)
  {
    ar & valid_mask;
    ar & txs_keeped_by_block;
  }
};

VARIANT_TAG(binary_archive, callback_entry, 0xcb);
VARIANT_TAG(binary_archive, cryptonote::account_base, 0xcc);
VARIANT_TAG(binary_archive, serialized_block, 0xcd);
VARIANT_TAG(binary_archive, serialized_transaction, 0xce);
VARIANT_TAG(binary_archive, event_visitor_settings, 0xcf);

typedef boost::variant<cryptonote::Block, cryptonote::Transaction, cryptonote::account_base, callback_entry, serialized_block, serialized_transaction, event_visitor_settings> test_event_entry;
typedef std::unordered_map<crypto::hash, const cryptonote::Transaction*> map_hash2tx_t;

class test_chain_unit_base: boost::noncopyable
{
public:
  test_chain_unit_base() :
    m_currency(cryptonote::CurrencyBuilder().currency()) {
  }

  typedef std::function<bool (cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)> verify_callback;
  typedef std::map<std::string, verify_callback> callbacks_map;

  const cryptonote::Currency& currency() const;
  void register_callback(const std::string& cb_name, verify_callback cb);
  bool verify(const std::string& cb_name, cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry> &events);

protected:
  cryptonote::Currency m_currency;

private:
  callbacks_map m_callbacks;
};


bool construct_tx_to_key(const std::vector<test_event_entry>& events, cryptonote::Transaction& tx,
                         const cryptonote::Block& blk_head, const cryptonote::account_base& from, const cryptonote::account_base& to,
                         uint64_t amount, uint64_t fee, size_t nmix);
cryptonote::Transaction construct_tx_with_fee(std::vector<test_event_entry>& events, const cryptonote::Block& blk_head,
                                            const cryptonote::account_base& acc_from, const cryptonote::account_base& acc_to,
                                            uint64_t amount, uint64_t fee);

void get_confirmed_txs(const std::vector<cryptonote::Block>& blockchain, const map_hash2tx_t& mtx, map_hash2tx_t& confirmed_txs);
bool find_block_chain(const std::vector<test_event_entry>& events, std::vector<cryptonote::Block>& blockchain, map_hash2tx_t& mtx, const crypto::hash& head);
void fill_tx_sources_and_destinations(const std::vector<test_event_entry>& events, const cryptonote::Block& blk_head,
                                      const cryptonote::account_base& from, const cryptonote::account_base& to,
                                      uint64_t amount, uint64_t fee, size_t nmix,
                                      std::vector<cryptonote::tx_source_entry>& sources,
                                      std::vector<cryptonote::tx_destination_entry>& destinations);
uint64_t get_balance(const cryptonote::account_base& addr, const std::vector<cryptonote::Block>& blockchain, const map_hash2tx_t& mtx);

//--------------------------------------------------------------------------
template<class t_test_class>
auto do_check_tx_verification_context(const cryptonote::tx_verification_context& tvc, bool tx_added, size_t event_index, const cryptonote::Transaction& tx, t_test_class& validator, int)
  -> decltype(validator.check_tx_verification_context(tvc, tx_added, event_index, tx))
{
  return validator.check_tx_verification_context(tvc, tx_added, event_index, tx);
}
//--------------------------------------------------------------------------
template<class t_test_class>
bool do_check_tx_verification_context(const cryptonote::tx_verification_context& tvc, bool tx_added, size_t /*event_index*/, const cryptonote::Transaction& /*tx*/, t_test_class&, long)
{
  // Default block verification context check
  if (tvc.m_verifivation_failed)
    throw std::runtime_error("Transaction verification failed");
  return true;
}
//--------------------------------------------------------------------------
template<class t_test_class>
bool check_tx_verification_context(const cryptonote::tx_verification_context& tvc, bool tx_added, size_t event_index, const cryptonote::Transaction& tx, t_test_class& validator)
{
  // SFINAE in action
  return do_check_tx_verification_context(tvc, tx_added, event_index, tx, validator, 0);
}
//--------------------------------------------------------------------------
template<class t_test_class>
auto do_check_block_verification_context(const cryptonote::block_verification_context& bvc, size_t event_index, const cryptonote::Block& blk, t_test_class& validator, int)
  -> decltype(validator.check_block_verification_context(bvc, event_index, blk))
{
  return validator.check_block_verification_context(bvc, event_index, blk);
}
//--------------------------------------------------------------------------
template<class t_test_class>
bool do_check_block_verification_context(const cryptonote::block_verification_context& bvc, size_t /*event_index*/, const cryptonote::Block& /*blk*/, t_test_class&, long)
{
  // Default block verification context check
  if (bvc.m_verifivation_failed)
    throw std::runtime_error("Block verification failed");
  return true;
}
//--------------------------------------------------------------------------
template<class t_test_class>
bool check_block_verification_context(const cryptonote::block_verification_context& bvc, size_t event_index, const cryptonote::Block& blk, t_test_class& validator)
{
  // SFINAE in action
  return do_check_block_verification_context(bvc, event_index, blk, validator, 0);
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
template<class t_test_class>
struct push_core_event_visitor: public boost::static_visitor<bool>
{
private:
  cryptonote::core& m_c;
  const std::vector<test_event_entry>& m_events;
  t_test_class& m_validator;
  size_t m_ev_index;

  bool m_txs_keeped_by_block;

public:
  push_core_event_visitor(cryptonote::core& c, const std::vector<test_event_entry>& events, t_test_class& validator)
    : m_c(c)
    , m_events(events)
    , m_validator(validator)
    , m_ev_index(0)
    , m_txs_keeped_by_block(false)
  {
  }

  void event_index(size_t ev_index)
  {
    m_ev_index = ev_index;
  }

  bool operator()(const event_visitor_settings& settings)
  {
    log_event("event_visitor_settings");

    if (settings.valid_mask & event_visitor_settings::set_txs_keeped_by_block)
    {
      m_txs_keeped_by_block = settings.txs_keeped_by_block;
    }

    return true;
  }

  bool operator()(const cryptonote::Transaction& tx) const
  {
    log_event("cryptonote::Transaction");

    cryptonote::tx_verification_context tvc = boost::value_initialized<decltype(tvc)>();
    size_t pool_size = m_c.get_pool_transactions_count();
    m_c.handle_incoming_tx(t_serializable_object_to_blob(tx), tvc, m_txs_keeped_by_block);
    bool tx_added = pool_size + 1 == m_c.get_pool_transactions_count();
    bool r = check_tx_verification_context(tvc, tx_added, m_ev_index, tx, m_validator);
    CHECK_AND_NO_ASSERT_MES(r, false, "tx verification context check failed");
    return true;
  }

  bool operator()(const cryptonote::Block& b) const
  {
    log_event("cryptonote::Block");

    cryptonote::block_verification_context bvc = boost::value_initialized<decltype(bvc)>();
    m_c.handle_incoming_block_blob(t_serializable_object_to_blob(b), bvc, false, false);
    bool r = check_block_verification_context(bvc, m_ev_index, b, m_validator);
    CHECK_AND_NO_ASSERT_MES(r, false, "block verification context check failed");
    return r;
  }

  bool operator()(const callback_entry& cb) const
  {
    log_event(std::string("callback_entry ") + cb.callback_name);
    return m_validator.verify(cb.callback_name, m_c, m_ev_index, m_events);
  }

  bool operator()(const cryptonote::account_base& ab) const
  {
    log_event("cryptonote::account_base");
    return true;
  }

  bool operator()(const serialized_block& sr_block) const
  {
    log_event("serialized_block");

    cryptonote::block_verification_context bvc = boost::value_initialized<decltype(bvc)>();
    m_c.handle_incoming_block_blob(sr_block.data, bvc, false, false);

    cryptonote::Block blk;
    std::stringstream ss;
    ss << sr_block.data;
    binary_archive<false> ba(ss);
    ::serialization::serialize(ba, blk);
    if (!ss.good())
    {
      blk = cryptonote::Block();
    }
    bool r = check_block_verification_context(bvc, m_ev_index, blk, m_validator);
    CHECK_AND_NO_ASSERT_MES(r, false, "block verification context check failed");
    return true;
  }

  bool operator()(const serialized_transaction& sr_tx) const
  {
    log_event("serialized_transaction");

    cryptonote::tx_verification_context tvc = boost::value_initialized<decltype(tvc)>();;
    size_t pool_size = m_c.get_pool_transactions_count();
    m_c.handle_incoming_tx(sr_tx.data, tvc, m_txs_keeped_by_block);
    bool tx_added = pool_size + 1 == m_c.get_pool_transactions_count();

    cryptonote::Transaction tx;
    std::stringstream ss;
    ss << sr_tx.data;
    binary_archive<false> ba(ss);
    ::serialization::serialize(ba, tx);
    if (!ss.good())
    {
      tx = cryptonote::Transaction();
    }

    bool r = check_tx_verification_context(tvc, tx_added, m_ev_index, tx, m_validator);
    CHECK_AND_NO_ASSERT_MES(r, false, "transaction verification context check failed");
    return true;
  }

private:
  void log_event(const std::string& event_type) const
  {
    std::cout << concolor::yellow << "=== EVENT # " << m_ev_index << ": " << event_type << concolor::normal << std::endl;
  }
};
//--------------------------------------------------------------------------
template<class t_test_class>
inline bool replay_events_through_core(cryptonote::core& cr, const std::vector<test_event_entry>& events, t_test_class& validator)
{
  TRY_ENTRY();

  //init core here

  CHECK_AND_ASSERT_MES(typeid(cryptonote::Block) == events[0].type(), false, "First event must be genesis block creation");
  cr.set_genesis_block(boost::get<cryptonote::Block>(events[0]));

  bool r = true;
  push_core_event_visitor<t_test_class> visitor(cr, events, validator);
  for(size_t i = 1; i < events.size() && r; ++i)
  {
    visitor.event_index(i);
    r = boost::apply_visitor(visitor, events[i]);
  }

  return r;

  CATCH_ENTRY_L0("replay_events_through_core", false);
}
//--------------------------------------------------------------------------
template<class t_test_class>
inline bool do_replay_events(std::vector<test_event_entry>& events, t_test_class& validator)
{
  boost::program_options::options_description desc("Allowed options");
  cryptonote::CoreConfig::initOptions(desc);
  command_line::add_arg(desc, command_line::arg_data_dir);
  boost::program_options::variables_map vm;
  bool r = command_line::handle_error_helper(desc, [&]()
  {
    boost::program_options::store(boost::program_options::basic_parsed_options<char>(&desc), vm);
    boost::program_options::notify(vm);
    return true;
  });
  if (!r)
    return false;

  cryptonote::CoreConfig coreConfig;
  coreConfig.init(vm);
  cryptonote::MinerConfig emptyMinerConfig;

  cryptonote::cryptonote_protocol_stub pr; //TODO: stub only for this kind of test, make real validation of relayed objects
  cryptonote::core c(validator.currency(), &pr);
  if (!c.init(coreConfig, emptyMinerConfig, false))
  {
    std::cout << concolor::magenta << "Failed to init core" << concolor::normal << std::endl;
    return false;
  }

  return replay_events_through_core<t_test_class>(c, events, validator);
}
//--------------------------------------------------------------------------
template<class t_test_class>
inline bool do_replay_file(const std::string& filename)
{
  std::vector<test_event_entry> events;
  if (!tools::unserialize_obj_from_file(events, filename))
  {
    std::cout << concolor::magenta << "Failed to deserialize data from file: " << filename << concolor::normal << std::endl;
    return false;
  }
  t_test_class validator;
  return do_replay_events<t_test_class>(events, validator);
}
//--------------------------------------------------------------------------
#define GENERATE_ACCOUNT(account) \
    cryptonote::account_base account; \
    account.generate();

#define MAKE_ACCOUNT(VEC_EVENTS, account) \
  cryptonote::account_base account; \
  account.generate(); \
  VEC_EVENTS.push_back(account);

#define DO_CALLBACK(VEC_EVENTS, CB_NAME) \
{ \
  callback_entry CALLBACK_ENTRY; \
  CALLBACK_ENTRY.callback_name = CB_NAME; \
  VEC_EVENTS.push_back(CALLBACK_ENTRY); \
}

#define REGISTER_CALLBACK(CB_NAME, CLBACK) \
  register_callback(CB_NAME, std::bind(&CLBACK, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

#define REGISTER_CALLBACK_METHOD(CLASS, METHOD) \
  register_callback(#METHOD, std::bind(&CLASS::METHOD, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

#define MAKE_GENESIS_BLOCK(VEC_EVENTS, BLK_NAME, MINER_ACC, TS)                       \
  test_generator generator(this->m_currency);                                         \
  cryptonote::Block BLK_NAME;                                                         \
  generator.constructBlock(BLK_NAME, MINER_ACC, TS);                                  \
  VEC_EVENTS.push_back(BLK_NAME);

#define MAKE_NEXT_BLOCK(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC)                  \
  cryptonote::Block BLK_NAME;                                                         \
  generator.constructBlock(BLK_NAME, PREV_BLOCK, MINER_ACC);                          \
  VEC_EVENTS.push_back(BLK_NAME);

#define MAKE_NEXT_BLOCK_TX1(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC, TX1)         \
  cryptonote::Block BLK_NAME;                                                         \
  {                                                                                   \
    std::list<cryptonote::Transaction> tx_list;                                       \
    tx_list.push_back(TX1);                                                           \
    generator.constructBlock(BLK_NAME, PREV_BLOCK, MINER_ACC, tx_list);               \
  }                                                                                   \
  VEC_EVENTS.push_back(BLK_NAME);

#define MAKE_NEXT_BLOCK_TX_LIST(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC, TXLIST)  \
  cryptonote::Block BLK_NAME;                                                         \
  generator.constructBlock(BLK_NAME, PREV_BLOCK, MINER_ACC, TXLIST);                  \
  VEC_EVENTS.push_back(BLK_NAME);

#define REWIND_BLOCKS_N(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC, COUNT)           \
  cryptonote::Block BLK_NAME;                                                         \
  {                                                                                   \
    cryptonote::Block blk_last = PREV_BLOCK;                                          \
    for (size_t i = 0; i < COUNT; ++i)                                                \
    {                                                                                 \
      MAKE_NEXT_BLOCK(VEC_EVENTS, blk, blk_last, MINER_ACC);                          \
      blk_last = blk;                                                                 \
    }                                                                                 \
    BLK_NAME = blk_last;                                                              \
  }

#define REWIND_BLOCKS(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC) \
  REWIND_BLOCKS_N(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC, this->m_currency.minedMoneyUnlockWindow())

#define MAKE_TX_MIX(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, NMIX, HEAD)                                   \
  cryptonote::Transaction TX_NAME;                                                                       \
  construct_tx_to_key(VEC_EVENTS, TX_NAME, HEAD, FROM, TO, AMOUNT, this->m_currency.minimumFee(), NMIX); \
  VEC_EVENTS.push_back(TX_NAME);

#define MAKE_TX(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, HEAD) MAKE_TX_MIX(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, 0, HEAD)

#define MAKE_TX_MIX_LIST(VEC_EVENTS, SET_NAME, FROM, TO, AMOUNT, NMIX, HEAD)                         \
  {                                                                                                  \
    cryptonote::Transaction t;                                                                       \
    construct_tx_to_key(VEC_EVENTS, t, HEAD, FROM, TO, AMOUNT, this->m_currency.minimumFee(), NMIX); \
    SET_NAME.push_back(t);                                                                           \
    VEC_EVENTS.push_back(t);                                                                         \
  }

#define MAKE_TX_LIST(VEC_EVENTS, SET_NAME, FROM, TO, AMOUNT, HEAD) MAKE_TX_MIX_LIST(VEC_EVENTS, SET_NAME, FROM, TO, AMOUNT, 0, HEAD)

#define MAKE_TX_LIST_START(VEC_EVENTS, SET_NAME, FROM, TO, AMOUNT, HEAD) \
    std::list<cryptonote::Transaction> SET_NAME; \
    MAKE_TX_LIST(VEC_EVENTS, SET_NAME, FROM, TO, AMOUNT, HEAD);

#define MAKE_MINER_TX_AND_KEY_MANUALLY(TX, BLK, KEY)                                                                  \
  Transaction TX;                                                                                                     \
  if (!constructMinerTxManually(this->m_currency, get_block_height(BLK) + 1, generator.getAlreadyGeneratedCoins(BLK), \
    miner_account.get_keys().m_account_address, TX, 0, KEY))                                                          \
    return false;

#define MAKE_MINER_TX_MANUALLY(TX, BLK) MAKE_MINER_TX_AND_KEY_MANUALLY(TX, BLK, 0)

#define SET_EVENT_VISITOR_SETT(VEC_EVENTS, SETT, VAL) VEC_EVENTS.push_back(event_visitor_settings(SETT, VAL));

#define GENERATE(filename, genclass) \
    { \
        std::vector<test_event_entry> events; \
        genclass g; \
        g.generate(events); \
        if (!tools::serialize_obj_to_file(events, filename)) \
        { \
            std::cout << concolor::magenta << "Failed to serialize data to file: " << filename << concolor::normal << std::endl; \
            throw std::runtime_error("Failed to serialize data to file"); \
        } \
    }


#define PLAY(filename, genclass) \
    if(!do_replay_file<genclass>(filename)) \
    { \
      std::cout << concolor::magenta << "Failed to pass test : " << #genclass << concolor::normal << std::endl; \
      return 1; \
    }

#define GENERATE_AND_PLAY(genclass)                                                                        \
  {                                                                                                        \
    std::vector<test_event_entry> events;                                                                  \
    ++tests_count;                                                                                         \
    bool generated = false;                                                                                \
    try                                                                                                    \
    {                                                                                                      \
      genclass g;                                                                                          \
      generated = g.generate(events);;                                                                     \
    }                                                                                                      \
    catch (const std::exception& ex)                                                                       \
    {                                                                                                      \
      LOG_PRINT(#genclass << " generation failed: what=" << ex.what(), 0);                                 \
    }                                                                                                      \
    catch (...)                                                                                            \
    {                                                                                                      \
      LOG_PRINT(#genclass << " generation failed: generic exception", 0);                                  \
    }                                                                                                      \
    genclass validator;                                                                                    \
    if (generated && do_replay_events< genclass >(events, validator))                                      \
    {                                                                                                      \
      std::cout << concolor::green << "#TEST# Succeeded " << #genclass << concolor::normal << '\n';        \
    }                                                                                                      \
    else                                                                                                   \
    {                                                                                                      \
      std::cout << concolor::magenta << "#TEST# Failed " << #genclass << concolor::normal << '\n';         \
      failed_tests.push_back(#genclass);                                                                   \
    }                                                                                                      \
    std::cout << std::endl;                                                                                \
  }


template <typename GenClassT>
bool GenerateAndPlay(const char* testname, GenClassT&& g) {
  std::vector<test_event_entry> events;
  bool generated = false;

  try {
    generated = g.generate(events);
  } catch (const std::exception& ex) {
    LOG_PRINT(testname << " generation failed: what=" << ex.what(), 0);
  } catch (...) {
    LOG_PRINT(testname << " generation failed: generic exception", 0);
  }

  bool succeeded = generated && do_replay_events<GenClassT>(events, g);

  if (succeeded) {
    std::cout << concolor::green << "#TEST# Succeeded " << testname << concolor::normal << '\n';
  } else {
    std::cout << concolor::magenta << "#TEST# Failed " << testname << concolor::normal << '\n';
  }

  std::cout << std::endl;
  return succeeded;
}

#define GENERATE_AND_PLAY_EX(genclass) { ++tests_count; if (!GenerateAndPlay(#genclass, genclass)) failed_tests.push_back(#genclass); }


#define CALL_TEST(test_name, function)                                                                     \
  {                                                                                                        \
    if(!function())                                                                                        \
    {                                                                                                      \
      std::cout << concolor::magenta << "#TEST# Failed " << test_name << concolor::normal << std::endl;    \
      return 1;                                                                                            \
    }                                                                                                      \
    else                                                                                                   \
    {                                                                                                      \
      std::cout << concolor::green << "#TEST# Succeeded " << test_name << concolor::normal << std::endl;   \
    }                                                                                                      \
  }

#define QUOTEME(x) #x
#define DEFINE_TESTS_ERROR_CONTEXT(text) const char* perr_context = text;
#define CHECK_TEST_CONDITION(cond) CHECK_AND_ASSERT_MES(cond, false, "[" << perr_context << "] failed: \"" << QUOTEME(cond) << "\"")
#define CHECK_EQ(v1, v2) CHECK_AND_ASSERT_MES(v1 == v2, false, "[" << perr_context << "] failed: \"" << QUOTEME(v1) << " == " << QUOTEME(v2) << "\", " << v1 << " != " << v2)
#define CHECK_NOT_EQ(v1, v2) CHECK_AND_ASSERT_MES(!(v1 == v2), false, "[" << perr_context << "] failed: \"" << QUOTEME(v1) << " != " << QUOTEME(v2) << "\", " << v1 << " == " << v2)
#define MK_COINS(amount) (UINT64_C(amount) * cryptonote::parameters::COIN)
