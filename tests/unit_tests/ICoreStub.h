// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>

#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/ICore.h"
#include "cryptonote_core/ICoreObserver.h"
#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "rpc/core_rpc_server_commands_defs.h"

class ICoreStub: public cryptonote::ICore {
public:
  ICoreStub() : topHeight(0), topResult(false), globalIndicesResult(false), randomOutsResult(false) {};

  virtual bool addObserver(cryptonote::ICoreObserver* observer);
  virtual bool removeObserver(cryptonote::ICoreObserver* observer);
  virtual bool get_blockchain_top(uint64_t& height, crypto::hash& top_id);
  virtual bool find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, std::list<std::pair<cryptonote::Block, std::list<cryptonote::Transaction> > >& blocks,
      uint64_t& total_height, uint64_t& start_height, size_t max_count);
  virtual bool find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, cryptonote::NOTIFY_RESPONSE_CHAIN_ENTRY_request& resp);
  virtual bool get_random_outs_for_amounts(const cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_request& req,
      cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response& res);
  virtual bool get_tx_outputs_gindexs(const crypto::hash& tx_id, std::vector<uint64_t>& indexs);
  virtual cryptonote::i_cryptonote_protocol* get_protocol();
  virtual bool handle_incoming_tx(cryptonote::blobdata const& tx_blob, cryptonote::tx_verification_context& tvc, bool keeped_by_block);
  virtual bool getPoolSymmetricDifference(const std::vector<crypto::hash>& known_pool_tx_ids, const crypto::hash& known_block_id, bool& isBcActual, std::vector<cryptonote::Transaction>& new_txs, std::vector<crypto::hash>& deleted_tx_ids) override;
  virtual bool queryBlocks(const std::list<crypto::hash>& block_ids, uint64_t timestamp,
      uint64_t& start_height, uint64_t& current_height, uint64_t& full_offset, std::list<cryptonote::BlockFullInfo>& entries);

  virtual bool getBlockByHash(const crypto::hash &h, cryptonote::Block &blk) override;

  void set_blockchain_top(uint64_t height, const crypto::hash& top_id, bool result);
  void set_outputs_gindexs(const std::vector<uint64_t>& indexs, bool result);
  void set_random_outs(const cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response& resp, bool result);

private:
  uint64_t topHeight;
  crypto::hash topId;
  bool topResult;

  std::vector<uint64_t> globalIndices;
  bool globalIndicesResult;

  cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response randomOuts;
  bool randomOutsResult;
};
