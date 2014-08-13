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

#include <unordered_map>

#include <boost/program_options/variables_map.hpp>

#include "cryptonote_core/cryptonote_basic_impl.h"
#include "cryptonote_core/Currency.h"
#include "cryptonote_core/verification_context.h"

namespace tests
{
  struct block_index {
      size_t height;
      crypto::hash id;
      crypto::hash longhash;
      cryptonote::Block blk;
      cryptonote::blobdata blob;
      std::list<cryptonote::Transaction> txes;

      block_index() : height(0), id(cryptonote::null_hash), longhash(cryptonote::null_hash) { }
      block_index(size_t _height, const crypto::hash &_id, const crypto::hash &_longhash, const cryptonote::Block &_blk, const cryptonote::blobdata &_blob, const std::list<cryptonote::Transaction> &_txes)
          : height(_height), id(_id), longhash(_longhash), blk(_blk), blob(_blob), txes(_txes) { }
  };

  class proxy_core
  {
    const cryptonote::Currency& m_currency;
    cryptonote::Block m_genesis;
    std::list<crypto::hash> m_known_block_list;
    std::unordered_map<crypto::hash, block_index> m_hash2blkidx;

    crypto::hash m_lastblk;
    std::list<cryptonote::Transaction> txes;

    crypto::cn_context m_cn_context;

    bool add_block(const crypto::hash &_id, const crypto::hash &_longhash, const cryptonote::Block &_blk, const cryptonote::blobdata &_blob);
    void build_short_history(std::list<crypto::hash> &m_history, const crypto::hash &m_start);


  public:
    proxy_core(const cryptonote::Currency& currency) : m_currency(currency) {
    }

    void on_synchronized(){}
    uint64_t get_current_blockchain_height(){return 1;}
    const cryptonote::Currency& currency() const { return m_currency; }
    bool init(const boost::program_options::variables_map& vm);
    bool deinit(){return true;}
    bool get_short_chain_history(std::list<crypto::hash>& ids);
    bool get_stat_info(cryptonote::core_stat_info& st_inf){return true;}
    bool have_block(const crypto::hash& id);
    bool get_blockchain_top(uint64_t& height, crypto::hash& top_id);
    bool handle_incoming_tx(const cryptonote::blobdata& tx_blob, cryptonote::tx_verification_context& tvc, bool keeped_by_block);
    bool handle_incoming_block_blob(const cryptonote::blobdata& block_blob, cryptonote::block_verification_context& bvc, bool control_miner, bool relay_block);
    void pause_mining(){}
    void update_block_template_and_resume_mining(){}
    bool on_idle(){return true;}
    bool find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, cryptonote::NOTIFY_RESPONSE_CHAIN_ENTRY::request& resp){return true;}
    bool handle_get_objects(cryptonote::NOTIFY_REQUEST_GET_OBJECTS::request& arg, cryptonote::NOTIFY_RESPONSE_GET_OBJECTS::request& rsp, cryptonote::cryptonote_connection_context& context){return true;}
  };
}
