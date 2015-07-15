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

#include "INode.h"
#include "cryptonote_protocol/ICryptonoteProtocolQuery.h"
#include "cryptonote_protocol/ICryptonoteProtocolObserver.h"
#include "cryptonote_core/ICore.h"
#include "cryptonote_core/ICoreObserver.h"
#include "Common/ObserverManager.h"
#include "BlockchainExplorer/BlockchainExplorerDataBuilder.h"

#include <thread>
#include <boost/asio.hpp>

namespace CryptoNote {

class core;

class InProcessNode : public INode, public CryptoNote::ICryptonoteProtocolObserver, public CryptoNote::ICoreObserver {
public:
  InProcessNode(CryptoNote::ICore& core, CryptoNote::ICryptonoteProtocolQuery& protocol);

  InProcessNode(const InProcessNode&) = delete;
  InProcessNode(InProcessNode&&) = delete;

  InProcessNode& operator=(const InProcessNode&) = delete;
  InProcessNode& operator=(InProcessNode&&) = delete;

  virtual ~InProcessNode();

  virtual void init(const Callback& callback) override;
  virtual bool shutdown() override;

  virtual bool addObserver(INodeObserver* observer) override;
  virtual bool removeObserver(INodeObserver* observer) override;

  virtual size_t getPeerCount() const;
  virtual uint64_t getLastLocalBlockHeight() const;
  virtual uint64_t getLastKnownBlockHeight() const;
  virtual uint64_t getLocalBlockCount() const override;
  virtual uint64_t getKnownBlockCount() const override;
  virtual uint64_t getLastLocalBlockTimestamp() const override;

  virtual void getNewBlocks(std::list<crypto::hash>&& knownBlockIds, std::list<CryptoNote::block_complete_entry>& newBlocks, uint64_t& startHeight, const Callback& callback) override;
  virtual void getTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices, const Callback& callback) override;
  virtual void getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount,
      std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback) override;
  virtual void relayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback) override;
  virtual void queryBlocks(std::list<crypto::hash>&& knownBlockIds, uint64_t timestamp, std::list<BlockCompleteEntry>& newBlocks, uint64_t& startHeight,
      const Callback& callback) override;
  virtual void getPoolSymmetricDifference(std::vector<crypto::hash>&& known_pool_tx_ids, crypto::hash known_block_id, bool& is_bc_actual, std::vector<CryptoNote::Transaction>& new_txs,
    std::vector<crypto::hash>& deleted_tx_ids, const Callback& callback) override;

  virtual void getBlocks(const std::vector<uint64_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks, const Callback& callback) override;
  virtual void getBlocks(const std::vector<crypto::hash>& blockHashes, std::vector<BlockDetails>& blocks, const Callback& callback) override;
  virtual void getTransactions(const std::vector<crypto::hash>& transactionHashes, std::vector<TransactionDetails>& transactions, const Callback& callback) override;
  virtual void isSynchronized(bool& syncStatus, const Callback& callback) override;

private:
  virtual void peerCountUpdated(size_t count) override;
  virtual void lastKnownBlockHeightUpdated(uint64_t height) override;
  virtual void blockchainSynchronized(uint64_t topHeight) override;
  virtual void blockchainUpdated() override;
  virtual void poolUpdated() override;

  void getNewBlocksAsync(std::list<crypto::hash>& knownBlockIds, std::list<CryptoNote::block_complete_entry>& newBlocks, uint64_t& startHeight, const Callback& callback);
  std::error_code doGetNewBlocks(std::list<crypto::hash>&& knownBlockIds, std::list<CryptoNote::block_complete_entry>& newBlocks, uint64_t& startHeight);

  void getTransactionOutsGlobalIndicesAsync(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices, const Callback& callback);
  std::error_code doGetTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices);

  void getRandomOutsByAmountsAsync(std::vector<uint64_t>& amounts, uint64_t outsCount,
      std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback);
  std::error_code doGetRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount,
      std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result);

  void relayTransactionAsync(const CryptoNote::Transaction& transaction, const Callback& callback);
  std::error_code doRelayTransaction(const CryptoNote::Transaction& transaction);

  void queryBlocksAsync(std::list<crypto::hash>& knownBlockIds, uint64_t timestamp, std::list<BlockCompleteEntry>& newBlocks, uint64_t& startHeight,
      const Callback& callback);
  std::error_code doQueryBlocks(std::list<crypto::hash>&& knownBlockIds, uint64_t timestamp, std::list<BlockCompleteEntry>& newBlocks, uint64_t& startHeight);

  void getPoolSymmetricDifferenceAsync(std::vector<crypto::hash>& known_pool_tx_ids, crypto::hash known_block_id, bool& is_bc_actual, std::vector<CryptoNote::Transaction>& new_txs,
    std::vector<crypto::hash>& deleted_tx_ids, const Callback& callback);

  void getBlocksAsync(const std::vector<uint64_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks, const Callback& callback);
  std::error_code doGetBlocks(const std::vector<uint64_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks);

  void getBlocksAsync(const std::vector<crypto::hash>& blockHashes, std::vector<BlockDetails>& blocks, const Callback& callback);
  std::error_code doGetBlocks(const std::vector<crypto::hash>& blockHashes, std::vector<BlockDetails>& blocks);

  void getTransactionsAsync(const std::vector<crypto::hash>& transactionHashes, std::vector<TransactionDetails>& transactions, const Callback& callback);
  std::error_code doGetTransactions(const std::vector<crypto::hash>& transactionHashes, std::vector<TransactionDetails>& transactions);

  void isSynchronizedAsync(bool& syncStatus, const Callback& callback);
  std::error_code doIsSynchronized(bool& syncStatus);

  void workerFunc();
  bool doShutdown();

  enum State {
    NOT_INITIALIZED,
    INITIALIZED
  };

  State state;
  CryptoNote::ICore& core;
  CryptoNote::ICryptonoteProtocolQuery& protocol;
  tools::ObserverManager<INodeObserver> observerManager;

  boost::asio::io_service ioService;
  std::unique_ptr<std::thread> workerThread;
  std::unique_ptr<boost::asio::io_service::work> work;

  BlockchainExplorerDataBuilder blockchainExplorerDataBuilder;

  mutable std::mutex mutex;
};

} //namespace CryptoNote
