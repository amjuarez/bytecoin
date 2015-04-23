// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ICoreStub.h"

bool ICoreStub::addObserver(cryptonote::ICoreObserver* observer) {
  return true;
}

bool ICoreStub::removeObserver(cryptonote::ICoreObserver* observer) {
  return true;
}

bool ICoreStub::get_blockchain_top(uint64_t& height, crypto::hash& top_id) {
  height = topHeight;
  top_id = topId;
  return topResult;
}

bool ICoreStub::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, std::list<std::pair<cryptonote::Block, std::list<cryptonote::Transaction> > >& blocks,
    uint64_t& total_height, uint64_t& start_height, size_t max_count)
{
  return true;
}

bool ICoreStub::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, cryptonote::NOTIFY_RESPONSE_CHAIN_ENTRY_request& resp) {
  return true;
}

bool ICoreStub::get_random_outs_for_amounts(const cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_request& req,
    cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response& res) {
  res = randomOuts;
  return randomOutsResult;
}

bool ICoreStub::get_tx_outputs_gindexs(const crypto::hash& tx_id, std::vector<uint64_t>& indexs) {
  std::copy(globalIndices.begin(), globalIndices.end(), std::back_inserter(indexs));
  return globalIndicesResult;
}

cryptonote::i_cryptonote_protocol* ICoreStub::get_protocol() {
  return nullptr;
}

bool ICoreStub::handle_incoming_tx(cryptonote::blobdata const& tx_blob, cryptonote::tx_verification_context& tvc, bool keeped_by_block) {
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

void ICoreStub::set_random_outs(const cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response& resp, bool result) {
  randomOuts = resp;
  randomOutsResult = result;
}

bool ICoreStub::getPoolSymmetricDifference(const std::vector<crypto::hash>& known_pool_tx_ids, const crypto::hash& known_block_id, bool& isBcActual, std::vector<cryptonote::Transaction>& new_txs, std::vector<crypto::hash>& deleted_tx_ids) {
  return true;
}

bool ICoreStub::queryBlocks(const std::list<crypto::hash>& block_ids, uint64_t timestamp,
    uint64_t& start_height, uint64_t& current_height, uint64_t& full_offset, std::list<cryptonote::BlockFullInfo>& entries) {
  //stub
  return true;
}

bool ICoreStub::getBlockByHash(const crypto::hash &h, cryptonote::Block &blk) {
  //stub
  return true;
}

