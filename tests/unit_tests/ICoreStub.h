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

#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/ICore.h"
#include "cryptonote_core/ICoreObserver.h"
#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "rpc/core_rpc_server_commands_defs.h"

class ICoreStub: public CryptoNote::ICore {
public:
  ICoreStub() : topHeight(0), topResult(false), globalIndicesResult(false), randomOutsResult(false) {};

  virtual bool addObserver(CryptoNote::ICoreObserver* observer);
  virtual bool removeObserver(CryptoNote::ICoreObserver* observer);
  virtual bool get_blockchain_top(uint64_t& height, crypto::hash& top_id);
  virtual bool find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, std::list<std::pair<CryptoNote::Block, std::list<CryptoNote::Transaction> > >& blocks,
      uint64_t& total_height, uint64_t& start_height, size_t max_count);
  virtual bool find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, CryptoNote::NOTIFY_RESPONSE_CHAIN_ENTRY_request& resp);
  virtual bool get_random_outs_for_amounts(const CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_request& req,
      CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response& res);
  virtual bool get_tx_outputs_gindexs(const crypto::hash& tx_id, std::vector<uint64_t>& indexs);
  virtual CryptoNote::i_cryptonote_protocol* get_protocol();
  virtual bool handle_incoming_tx(CryptoNote::blobdata const& tx_blob, CryptoNote::tx_verification_context& tvc, bool keeped_by_block);
  virtual bool getPoolSymmetricDifference(const std::vector<crypto::hash>& known_pool_tx_ids, const crypto::hash& known_block_id, bool& isBcActual, std::vector<CryptoNote::Transaction>& new_txs, std::vector<crypto::hash>& deleted_tx_ids) override;
  virtual bool queryBlocks(const std::list<crypto::hash>& block_ids, uint64_t timestamp,
      uint64_t& start_height, uint64_t& current_height, uint64_t& full_offset, std::list<CryptoNote::BlockFullInfo>& entries);

  virtual bool getBlockByHash(const crypto::hash &h, CryptoNote::Block &blk) override;

  virtual bool have_block(const crypto::hash& id) override { return false; }
  virtual bool get_short_chain_history(std::list<crypto::hash>& ids) override { return false; }
  virtual bool get_stat_info(CryptoNote::core_stat_info& st_inf) override { return false; }
  virtual bool on_idle() override { return false; }
  virtual void pause_mining() override {}
  virtual void update_block_template_and_resume_mining() override {}
  virtual bool handle_incoming_block_blob(const CryptoNote::blobdata& block_blob, CryptoNote::block_verification_context& bvc, bool control_miner, bool relay_block) override { return false; }
  virtual bool handle_get_objects(CryptoNote::NOTIFY_REQUEST_GET_OBJECTS::request& arg, CryptoNote::NOTIFY_RESPONSE_GET_OBJECTS::request& rsp) override { return false; }
  virtual void on_synchronized() override {}
  virtual bool is_ready() override { return true; }

  void set_blockchain_top(uint64_t height, const crypto::hash& top_id, bool result);
  void set_outputs_gindexs(const std::vector<uint64_t>& indexs, bool result);
  void set_random_outs(const CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response& resp, bool result);

private:
  uint64_t topHeight;
  crypto::hash topId;
  bool topResult;

  std::vector<uint64_t> globalIndices;
  bool globalIndicesResult;

  CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response randomOuts;
  bool randomOutsResult;
};
