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

#include "ICoreStub.h"

#include "cryptonote_core/cryptonote_format_utils.h"

bool ICoreStub::addObserver(CryptoNote::ICoreObserver* observer) {
  return true;
}

bool ICoreStub::removeObserver(CryptoNote::ICoreObserver* observer) {
  return true;
}

bool ICoreStub::get_blockchain_top(uint64_t& height, crypto::hash& top_id) {
  height = topHeight;
  top_id = topId;
  return topResult;
}

bool ICoreStub::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, std::list<std::pair<CryptoNote::Block, std::list<CryptoNote::Transaction> > >& blocks,
    uint64_t& total_height, uint64_t& start_height, size_t max_count)
{
  return true;
}

bool ICoreStub::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, CryptoNote::NOTIFY_RESPONSE_CHAIN_ENTRY_request& resp) {
  return true;
}

bool ICoreStub::get_random_outs_for_amounts(const CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_request& req,
    CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response& res) {
  res = randomOuts;
  return randomOutsResult;
}

bool ICoreStub::get_tx_outputs_gindexs(const crypto::hash& tx_id, std::vector<uint64_t>& indexs) {
  std::copy(globalIndices.begin(), globalIndices.end(), std::back_inserter(indexs));
  return globalIndicesResult;
}

CryptoNote::i_cryptonote_protocol* ICoreStub::get_protocol() {
  return nullptr;
}

bool ICoreStub::handle_incoming_tx(CryptoNote::blobdata const& tx_blob, CryptoNote::tx_verification_context& tvc, bool keeped_by_block) {
  return true;
}

void ICoreStub::set_blockchain_top(uint64_t height, const crypto::hash& top_id, bool result) {
  topHeight = height;
  topId = top_id;
  topResult = result;
}

void ICoreStub::set_outputs_gindexs(const std::vector<uint64_t>& indexs, bool result) {
  globalIndices.clear();
  std::copy(indexs.begin(), indexs.end(), std::back_inserter(globalIndices));
  globalIndicesResult = result;
}

void ICoreStub::set_random_outs(const CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response& resp, bool result) {
  randomOuts = resp;
  randomOutsResult = result;
}

std::vector<CryptoNote::Transaction> ICoreStub::getPoolTransactions() {
  return std::vector<CryptoNote::Transaction>();
}

bool ICoreStub::getPoolChanges(const crypto::hash& tailBlockId, const std::vector<crypto::hash>& knownTxsIds,
                               std::vector<CryptoNote::Transaction>& addedTxs, std::vector<crypto::hash>& deletedTxsIds) {
  return true;
}

void ICoreStub::getPoolChanges(const std::vector<crypto::hash>& knownTxsIds, std::vector<CryptoNote::Transaction>& addedTxs,
                               std::vector<crypto::hash>& deletedTxsIds) {
}

bool ICoreStub::queryBlocks(const std::list<crypto::hash>& block_ids, uint64_t timestamp,
    uint64_t& start_height, uint64_t& current_height, uint64_t& full_offset, std::list<CryptoNote::BlockFullInfo>& entries) {
  //stub
  return true;
}

crypto::hash ICoreStub::getBlockIdByHeight(uint64_t height) {
  auto iter = blockHashByHeightIndex.find(height);
  if (iter == blockHashByHeightIndex.end()) {
    return boost::value_initialized<crypto::hash>();
  }
  return iter->second;
}

bool ICoreStub::getBlockByHash(const crypto::hash &h, CryptoNote::Block &blk) {
  auto iter = blocks.find(h);
  if (iter == blocks.end()) {
    return false;
  }
  blk = iter->second;
  return true;
}

void ICoreStub::getTransactions(const std::vector<crypto::hash>& txs_ids, std::list<CryptoNote::Transaction>& txs, std::list<crypto::hash>& missed_txs, bool checkTxPool) {
  for (const crypto::hash& hash : txs_ids) {
    auto iter = transactions.find(hash);
    if (iter != transactions.end()) {
      txs.push_back(iter->second);
    } else {
      missed_txs.push_back(hash);
    }
  }
}

bool ICoreStub::getBackwardBlocksSizes(uint64_t fromHeight, std::vector<size_t>& sizes, size_t count) {
  return true;
}

bool ICoreStub::getBlockSize(const crypto::hash& hash, size_t& size) {
  return true;
}

bool ICoreStub::getAlreadyGeneratedCoins(const crypto::hash& hash, uint64_t& generatedCoins) {
  return true;
}

bool ICoreStub::getBlockReward(size_t medianSize, size_t currentBlockSize, uint64_t alreadyGeneratedCoins, uint64_t fee,
    bool penalizeFee, uint64_t& reward, int64_t& emissionChange) {
  return true;
}

bool ICoreStub::scanOutputkeysForIndices(const CryptoNote::TransactionInputToKey& txInToKey, std::list<std::pair<crypto::hash, size_t>>& outputReferences) {
  return true;
}

bool ICoreStub::getBlockDifficulty(uint64_t height, CryptoNote::difficulty_type& difficulty) {
  return true;
}

bool ICoreStub::getBlockContainingTx(const crypto::hash& txId, crypto::hash& blockId, uint64_t& blockHeight) {
  auto iter = blockHashByTxHashIndex.find(txId);
  if (iter == blockHashByTxHashIndex.end()) {
    return false;
  }
  blockId = iter->second;
  auto blockIter = blocks.find(blockId);
  if (blockIter == blocks.end()) {
    return false;
  }
  blockHeight = boost::get<CryptoNote::TransactionInputGenerate>(blockIter->second.minerTx.vin.front()).height;
  return true;
}

bool ICoreStub::getMultisigOutputReference(const CryptoNote::TransactionInputMultisignature& txInMultisig, std::pair<crypto::hash, size_t>& outputReference) {
  return true;
}

void ICoreStub::addBlock(const CryptoNote::Block& block) {
  uint64_t height = boost::get<CryptoNote::TransactionInputGenerate>(block.minerTx.vin.front()).height;
  crypto::hash hash = CryptoNote::get_block_hash(block);
  if (height > topHeight) {
    topHeight = height;
    topId = hash;
  }
  blocks.emplace(std::make_pair(hash, block));
  blockHashByHeightIndex.emplace(std::make_pair(height, hash));

  blockHashByTxHashIndex.emplace(std::make_pair(CryptoNote::get_transaction_hash(block.minerTx), hash));
  for (auto txHash : block.txHashes) {
    blockHashByTxHashIndex.emplace(std::make_pair(txHash, hash));
  }
}

void ICoreStub::addTransaction(const CryptoNote::Transaction& tx) {
  crypto::hash hash = CryptoNote::get_transaction_hash(tx);
  transactions.emplace(std::make_pair(hash, tx));
}
