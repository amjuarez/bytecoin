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

#include <cstdint>
#include <functional>
#include <system_error>
#include <vector>

#include "crypto/crypto.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "rpc/core_rpc_server_commands_defs.h"

namespace CryptoNote {

class INodeObserver {
public:
  virtual ~INodeObserver() {}
  virtual void peerCountUpdated(size_t count) {}
  virtual void localBlockchainUpdated(uint64_t height) {}
  virtual void lastKnownBlockHeightUpdated(uint64_t height) {}
  virtual void poolChanged() {}
};

struct OutEntry {
  uint64_t outGlobalIndex;
  crypto::public_key outKey;
};

struct OutsForAmount {
  uint64_t amount;
  std::vector<OutEntry> outs;
};

struct BlockCompleteEntry {
  crypto::hash blockHash;
  cryptonote::blobdata block;
  std::list<cryptonote::blobdata> txs;
};

class INode {
public:
  typedef std::function<void(std::error_code)> Callback;

  virtual ~INode() {}
  virtual bool addObserver(INodeObserver* observer) = 0;
  virtual bool removeObserver(INodeObserver* observer) = 0;

  virtual void init(const Callback& callback) = 0;
  virtual bool shutdown() = 0;

  virtual size_t getPeerCount() const = 0;
  virtual uint64_t getLastLocalBlockHeight() const = 0;
  virtual uint64_t getLastKnownBlockHeight() const = 0;
  virtual uint64_t getLastLocalBlockTimestamp() const = 0;

  virtual void relayTransaction(const cryptonote::Transaction& transaction, const Callback& callback) = 0;
  virtual void getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount, std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback) = 0;
  virtual void getNewBlocks(std::list<crypto::hash>&& knownBlockIds, std::list<cryptonote::block_complete_entry>& newBlocks, uint64_t& startHeight, const Callback& callback) = 0;
  virtual void getTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices, const Callback& callback) = 0;
  virtual void queryBlocks(std::list<crypto::hash>&& knownBlockIds, uint64_t timestamp, std::list<BlockCompleteEntry>& newBlocks, uint64_t& startHeight, const Callback& callback) = 0;
  virtual void getPoolSymmetricDifference(std::vector<crypto::hash>&& known_pool_tx_ids, crypto::hash known_block_id, bool& is_bc_actual, std::vector<cryptonote::Transaction>& new_txs, std::vector<crypto::hash>& deleted_tx_ids, const Callback& callback) = 0;
};

}
