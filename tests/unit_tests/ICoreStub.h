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
#include <unordered_map>

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
  virtual std::vector<CryptoNote::Transaction> getPoolTransactions() override;
  virtual bool getPoolChanges(const crypto::hash& tailBlockId, const std::vector<crypto::hash>& knownTxsIds,
                              std::vector<CryptoNote::Transaction>& addedTxs, std::vector<crypto::hash>& deletedTxsIds) override;
  virtual void getPoolChanges(const std::vector<crypto::hash>& knownTxsIds, std::vector<CryptoNote::Transaction>& addedTxs,
                              std::vector<crypto::hash>& deletedTxsIds) override;
  virtual bool queryBlocks(const std::list<crypto::hash>& block_ids, uint64_t timestamp,
      uint64_t& start_height, uint64_t& current_height, uint64_t& full_offset, std::list<CryptoNote::BlockFullInfo>& entries);

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

  virtual crypto::hash getBlockIdByHeight(uint64_t height) override;
  virtual bool getBlockByHash(const crypto::hash &h, CryptoNote::Block &blk) override;
  virtual void getTransactions(const std::vector<crypto::hash>& txs_ids, std::list<CryptoNote::Transaction>& txs, std::list<crypto::hash>& missed_txs, bool checkTxPool = false) override;
  virtual bool getBackwardBlocksSizes(uint64_t fromHeight, std::vector<size_t>& sizes, size_t count) override;
  virtual bool getBlockSize(const crypto::hash& hash, size_t& size) override;
  virtual bool getAlreadyGeneratedCoins(const crypto::hash& hash, uint64_t& generatedCoins) override;
  virtual bool getBlockReward(size_t medianSize, size_t currentBlockSize, uint64_t alreadyGeneratedCoins, uint64_t fee,
      bool penalizeFee, uint64_t& reward, int64_t& emissionChange) override;
  virtual bool scanOutputkeysForIndices(const CryptoNote::TransactionInputToKey& txInToKey, std::list<std::pair<crypto::hash, size_t>>& outputReferences) override;
  virtual bool getBlockDifficulty(uint64_t height, CryptoNote::difficulty_type& difficulty) override;
  virtual bool getBlockContainingTx(const crypto::hash& txId, crypto::hash& blockId, uint64_t& blockHeight) override;
  virtual bool getMultisigOutputReference(const CryptoNote::TransactionInputMultisignature& txInMultisig, std::pair<crypto::hash, size_t>& outputReference) override;

  void set_blockchain_top(uint64_t height, const crypto::hash& top_id, bool result);
  void set_outputs_gindexs(const std::vector<uint64_t>& indexs, bool result);
  void set_random_outs(const CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response& resp, bool result);

  void addBlock(const CryptoNote::Block& block);
  void addTransaction(const CryptoNote::Transaction& tx);

private:
  uint64_t topHeight;
  crypto::hash topId;
  bool topResult;

  std::vector<uint64_t> globalIndices;
  bool globalIndicesResult;

  CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response randomOuts;
  bool randomOutsResult;

  std::unordered_map<crypto::hash, CryptoNote::Block> blocks;
  std::unordered_map<uint64_t, crypto::hash> blockHashByHeightIndex;
  std::unordered_map<crypto::hash, crypto::hash> blockHashByTxHashIndex;

  std::unordered_map<crypto::hash, CryptoNote::Transaction> transactions;

};
