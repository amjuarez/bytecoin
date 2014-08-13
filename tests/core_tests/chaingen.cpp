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

#include "chaingen.h"

#include <vector>
#include <iostream>
#include <stdint.h>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/program_options.hpp>

#include "include_base_utils.h"
#include "misc_language.h"

#include "common/command_line.h"
#include "cryptonote_core/account_boost_serialization.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/cryptonote_basic_impl.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_core/cryptonote_boost_serialization.h"
#include "cryptonote_core/Currency.h"
#include "cryptonote_core/UpgradeDetector.h"

using namespace std;

using namespace epee;
using namespace cryptonote;


struct output_index {
    const cryptonote::TransactionOutputTarget out;
    uint64_t amount;
    size_t blk_height; // block height
    size_t tx_no; // index of transaction in block
    size_t out_no; // index of out in transaction
    size_t idx;
    bool spent;
    const cryptonote::Block *p_blk;
    const cryptonote::Transaction *p_tx;

    output_index(const cryptonote::TransactionOutputTarget &_out, uint64_t _a, size_t _h, size_t tno, size_t ono, const cryptonote::Block *_pb, const cryptonote::Transaction *_pt)
        : out(_out), amount(_a), blk_height(_h), tx_no(tno), out_no(ono), idx(0), spent(false), p_blk(_pb), p_tx(_pt) { }

    output_index(const output_index &other)
        : out(other.out), amount(other.amount), blk_height(other.blk_height), tx_no(other.tx_no), out_no(other.out_no), idx(other.idx), spent(other.spent), p_blk(other.p_blk), p_tx(other.p_tx) {  }

    const std::string toString() const {
        std::stringstream ss;

        ss << "output_index{blk_height=" << blk_height
           << " tx_no=" << tx_no
           << " out_no=" << out_no
           << " amount=" << amount
           << " idx=" << idx
           << " spent=" << spent
           << "}";

        return ss.str();
    }

    output_index& operator=(const output_index& other)
    {
      new(this) output_index(other);
      return *this;
    }
};

typedef std::map<uint64_t, std::vector<size_t> > map_output_t;
typedef std::map<uint64_t, std::vector<output_index> > map_output_idx_t;
typedef pair<uint64_t, size_t>  outloc_t;

namespace
{
  uint64_t get_inputs_amount(const vector<tx_source_entry> &s)
  {
    uint64_t r = 0;
    BOOST_FOREACH(const tx_source_entry &e, s)
    {
      r += e.amount;
    }

    return r;
  }
}

bool init_output_indices(map_output_idx_t& outs, std::map<uint64_t, std::vector<size_t> >& outs_mine, const std::vector<cryptonote::Block>& blockchain, const map_hash2tx_t& mtx, const cryptonote::account_base& from) {

    BOOST_FOREACH (const Block& blk, blockchain) {
        vector<const Transaction*> vtx;
        vtx.push_back(&blk.minerTx);

        for (const crypto::hash& h : blk.txHashes) {
            const map_hash2tx_t::const_iterator cit = mtx.find(h);
            if (mtx.end() == cit)
                throw std::runtime_error("block contains an unknown tx hash");

            vtx.push_back(cit->second);
        }

        //vtx.insert(vtx.end(), blk.);
        // TODO: add all other txes
        for (size_t i = 0; i < vtx.size(); i++) {
            const Transaction &tx = *vtx[i];

            size_t keyIndex = 0;
            for (size_t j = 0; j < tx.vout.size(); ++j) {
              const TransactionOutput &out = tx.vout[j];
              if (out.target.type() == typeid(TransactionOutputToKey)) {
                output_index oi(out.target, out.amount, boost::get<TransactionInputGenerate>(*blk.minerTx.vin.begin()).height, i, j, &blk, vtx[i]);
                outs[out.amount].push_back(oi);
                size_t tx_global_idx = outs[out.amount].size() - 1;
                outs[out.amount][tx_global_idx].idx = tx_global_idx;
                // Is out to me?
                if (is_out_to_acc(from.get_keys(), boost::get<TransactionOutputToKey>(out.target), get_tx_pub_key_from_extra(tx), keyIndex)) {
                  outs_mine[out.amount].push_back(tx_global_idx);
                }

                ++keyIndex;
              } else if (out.target.type() == typeid(TransactionOutputMultisignature)) {
                keyIndex += boost::get<TransactionOutputMultisignature>(out.target).keys.size();
              }
            }
        }
    }

    return true;
}

bool init_spent_output_indices(map_output_idx_t& outs, map_output_t& outs_mine, const std::vector<cryptonote::Block>& blockchain, const map_hash2tx_t& mtx, const cryptonote::account_base& from) {

    for (const map_output_t::value_type& o: outs_mine) {
        for (size_t i = 0; i < o.second.size(); ++i) {
            output_index &oi = outs[o.first][o.second[i]];

            // construct key image for this output
            crypto::key_image img;
            KeyPair in_ephemeral;
            generate_key_image_helper(from.get_keys(), get_tx_pub_key_from_extra(*oi.p_tx), oi.out_no, in_ephemeral, img);

            // lookup for this key image in the events vector
            for (auto& tx_pair : mtx) {
                const Transaction& tx = *tx_pair.second;
                for (const auto& in : tx.vin) {
                    if (typeid(TransactionInputToKey) == in.type()) {
                        const TransactionInputToKey &itk = boost::get<TransactionInputToKey>(in);
                        if (itk.keyImage == img) {
                            oi.spent = true;
                        }
                    }
                }
            }
        }
    }

    return true;
}

bool fill_output_entries(std::vector<output_index>& out_indices, size_t sender_out, size_t nmix, size_t& real_entry_idx, std::vector<tx_source_entry::output_entry>& output_entries)
{
  if (out_indices.size() <= nmix)
    return false;

  bool sender_out_found = false;
  size_t rest = nmix;
  for (size_t i = 0; i < out_indices.size() && (0 < rest || !sender_out_found); ++i)
  {
    const output_index& oi = out_indices[i];
    if (oi.spent)
      continue;

    bool append = false;
    if (i == sender_out)
    {
      append = true;
      sender_out_found = true;
      real_entry_idx = output_entries.size();
    }
    else if (0 < rest)
    {
      --rest;
      append = true;
    }

    if (append)
    {
      const TransactionOutputToKey& otk = boost::get<TransactionOutputToKey>(oi.out);
      output_entries.push_back(tx_source_entry::output_entry(oi.idx, otk.key));
    }
  }

  return 0 == rest && sender_out_found;
}

bool fill_tx_sources(std::vector<tx_source_entry>& sources, const std::vector<test_event_entry>& events,
                     const Block& blk_head, const cryptonote::account_base& from, uint64_t amount, size_t nmix)
{
    map_output_idx_t outs;
    map_output_t outs_mine;

    std::vector<cryptonote::Block> blockchain;
    map_hash2tx_t mtx;
    if (!find_block_chain(events, blockchain, mtx, get_block_hash(blk_head)))
        return false;

    if (!init_output_indices(outs, outs_mine, blockchain, mtx, from))
        return false;

    if (!init_spent_output_indices(outs, outs_mine, blockchain, mtx, from))
        return false;

    // Iterate in reverse is more efficiency
    uint64_t sources_amount = 0;
    bool sources_found = false;
    BOOST_REVERSE_FOREACH(const map_output_t::value_type o, outs_mine)
    {
        for (size_t i = 0; i < o.second.size() && !sources_found; ++i)
        {
            size_t sender_out = o.second[i];
            const output_index& oi = outs[o.first][sender_out];
            if (oi.spent)
                continue;

            cryptonote::tx_source_entry ts;
            ts.amount = oi.amount;
            ts.real_output_in_tx_index = oi.out_no;
            ts.real_out_tx_key = get_tx_pub_key_from_extra(*oi.p_tx); // incoming tx public key
            size_t realOutput;
            if (!fill_output_entries(outs[o.first], sender_out, nmix, realOutput, ts.outputs))
              continue;

            ts.real_output = realOutput;

            sources.push_back(ts);

            sources_amount += ts.amount;
            sources_found = amount <= sources_amount;
        }

        if (sources_found)
            break;
    }

    return sources_found;
}

bool fill_tx_destination(tx_destination_entry &de, const cryptonote::account_base &to, uint64_t amount) {
    de.addr = to.get_keys().m_account_address;
    de.amount = amount;
    return true;
}

void fill_tx_sources_and_destinations(const std::vector<test_event_entry>& events, const Block& blk_head,
                                      const cryptonote::account_base& from, const cryptonote::account_base& to,
                                      uint64_t amount, uint64_t fee, size_t nmix, std::vector<tx_source_entry>& sources,
                                      std::vector<tx_destination_entry>& destinations)
{
  sources.clear();
  destinations.clear();

  if (!fill_tx_sources(sources, events, blk_head, from, amount + fee, nmix))
    throw std::runtime_error("couldn't fill transaction sources");

  tx_destination_entry de;
  if (!fill_tx_destination(de, to, amount))
    throw std::runtime_error("couldn't fill transaction destination");
  destinations.push_back(de);

  tx_destination_entry de_change;
  uint64_t cache_back = get_inputs_amount(sources) - (amount + fee);
  if (0 < cache_back)
  {
    if (!fill_tx_destination(de_change, from, cache_back))
      throw std::runtime_error("couldn't fill transaction cache back destination");
    destinations.push_back(de_change);
  }
}

bool construct_tx_to_key(const std::vector<test_event_entry>& events, cryptonote::Transaction& tx, const Block& blk_head,
                         const cryptonote::account_base& from, const cryptonote::account_base& to, uint64_t amount,
                         uint64_t fee, size_t nmix)
{
  vector<tx_source_entry> sources;
  vector<tx_destination_entry> destinations;
  fill_tx_sources_and_destinations(events, blk_head, from, to, amount, fee, nmix, sources, destinations);

  return construct_tx(from.get_keys(), sources, destinations, std::vector<uint8_t>(), tx, 0);
}

Transaction construct_tx_with_fee(std::vector<test_event_entry>& events, const Block& blk_head,
                                  const account_base& acc_from, const account_base& acc_to, uint64_t amount, uint64_t fee)
{
  Transaction tx;
  construct_tx_to_key(events, tx, blk_head, acc_from, acc_to, amount, fee, 0);
  events.push_back(tx);
  return tx;
}

uint64_t get_balance(const cryptonote::account_base& addr, const std::vector<cryptonote::Block>& blockchain, const map_hash2tx_t& mtx) {
    uint64_t res = 0;
    std::map<uint64_t, std::vector<output_index> > outs;
    std::map<uint64_t, std::vector<size_t> > outs_mine;

    map_hash2tx_t confirmed_txs;
    get_confirmed_txs(blockchain, mtx, confirmed_txs);

    if (!init_output_indices(outs, outs_mine, blockchain, confirmed_txs, addr))
        return false;

    if (!init_spent_output_indices(outs, outs_mine, blockchain, confirmed_txs, addr))
        return false;

    BOOST_FOREACH (const map_output_t::value_type &o, outs_mine) {
        for (size_t i = 0; i < o.second.size(); ++i) {
            if (outs[o.first][o.second[i]].spent)
                continue;

            res += outs[o.first][o.second[i]].amount;
        }
    }

    return res;
}

void get_confirmed_txs(const std::vector<cryptonote::Block>& blockchain, const map_hash2tx_t& mtx, map_hash2tx_t& confirmed_txs)
{
  std::unordered_set<crypto::hash> confirmed_hashes;
  for (const Block& blk : blockchain)
  {
    for (const crypto::hash& tx_hash : blk.txHashes)
    {
      confirmed_hashes.insert(tx_hash);
    }
  }

  BOOST_FOREACH(const auto& tx_pair, mtx)
  {
    if (0 != confirmed_hashes.count(tx_pair.first))
    {
      confirmed_txs.insert(tx_pair);
    }
  }
}

bool find_block_chain(const std::vector<test_event_entry>& events, std::vector<cryptonote::Block>& blockchain, map_hash2tx_t& mtx, const crypto::hash& head) {
    std::unordered_map<crypto::hash, const Block*> block_index;
    BOOST_FOREACH(const test_event_entry& ev, events)
    {
        if (typeid(Block) == ev.type())
        {
            const Block* blk = &boost::get<Block>(ev);
            block_index[get_block_hash(*blk)] = blk;
        }
        else if (typeid(Transaction) == ev.type())
        {
            const Transaction& tx = boost::get<Transaction>(ev);
            mtx[get_transaction_hash(tx)] = &tx;
        }
    }

    bool b_success = false;
    crypto::hash id = head;
    for (auto it = block_index.find(id); block_index.end() != it; it = block_index.find(id))
    {
        blockchain.push_back(*it->second);
        id = it->second->prevId;
        if (null_hash == id)
        {
            b_success = true;
            break;
        }
    }
    reverse(blockchain.begin(), blockchain.end());

    return b_success;
}


const cryptonote::Currency& test_chain_unit_base::currency() const
{
  return m_currency;
}

void test_chain_unit_base::register_callback(const std::string& cb_name, verify_callback cb)
{
  m_callbacks[cb_name] = cb;
}

bool test_chain_unit_base::verify(const std::string& cb_name, cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  auto cb_it = m_callbacks.find(cb_name);
  if(cb_it == m_callbacks.end())
  {
    LOG_ERROR("Failed to find callback " << cb_name);
    return false;
  }
  return cb_it->second(c, ev_index, events);
}
