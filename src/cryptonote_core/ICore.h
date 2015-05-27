// Copyright (c) 2012-2015, The CryptoNote developers, The Bytecoin developers
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

#include <cstdint>
#include <list>
#include <utility>
#include <vector>

#include "crypto/hash.h"
#include "cryptonote_protocol/blobdatatype.h"
#include "cryptonote_protocol/cryptonote_protocol_defs.h"

namespace CryptoNote {

struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_request;
struct COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response;

struct Block;
struct Transaction;
struct i_cryptonote_protocol;
struct tx_verification_context;
struct block_verification_context;
struct core_stat_info;
struct BlockFullInfo;
class ICoreObserver;
class Currency;

class ICore {
public:
  virtual ~ICore() {}

  virtual bool addObserver(ICoreObserver* observer) = 0;
  virtual bool removeObserver(ICoreObserver* observer) = 0;

  virtual bool have_block(const crypto::hash& id) = 0;
  virtual bool get_short_chain_history(std::list<crypto::hash>& ids) = 0;
  virtual bool get_stat_info(CryptoNote::core_stat_info& st_inf) = 0;
  virtual bool on_idle() = 0;
  virtual void pause_mining() = 0;
  virtual void update_block_template_and_resume_mining() = 0;
  virtual bool handle_incoming_block_blob(const CryptoNote::blobdata& block_blob, CryptoNote::block_verification_context& bvc, bool control_miner, bool relay_block) = 0;
  virtual bool handle_get_objects(CryptoNote::NOTIFY_REQUEST_GET_OBJECTS::request& arg, CryptoNote::NOTIFY_RESPONSE_GET_OBJECTS::request& rsp) = 0;
  virtual void on_synchronized() = 0;
  virtual bool is_ready() = 0;

  virtual bool get_blockchain_top(uint64_t& height, crypto::hash& top_id) = 0;
  virtual bool find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, std::list<std::pair<Block, std::list<Transaction> > >& blocks,
      uint64_t& total_height, uint64_t& start_height, size_t max_count) = 0;
  virtual bool find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, NOTIFY_RESPONSE_CHAIN_ENTRY_request& resp) = 0;
  virtual bool get_random_outs_for_amounts(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response& res) = 0;
  virtual bool get_tx_outputs_gindexs(const crypto::hash& tx_id, std::vector<uint64_t>& indexs) = 0;
  virtual i_cryptonote_protocol* get_protocol() = 0;
  virtual bool handle_incoming_tx(const blobdata& tx_blob, tx_verification_context& tvc, bool keeped_by_block) = 0;
  virtual bool getPoolSymmetricDifference(const std::vector<crypto::hash>& known_pool_tx_ids, const crypto::hash& known_block_id, bool& isBcActual, std::vector<Transaction>& new_txs, std::vector<crypto::hash>& deleted_tx_ids) = 0;
  virtual bool queryBlocks(const std::list<crypto::hash>& block_ids, uint64_t timestamp,
      uint64_t& start_height, uint64_t& current_height, uint64_t& full_offset, std::list<BlockFullInfo>& entries) = 0;

  virtual bool getBlockByHash(const crypto::hash &h, Block &blk) = 0;
};

} //namespace CryptoNote
