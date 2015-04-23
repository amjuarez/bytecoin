// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "INode.h"
#include "cryptonote_protocol/ICryptonoteProtocolQuery.h"
#include "cryptonote_protocol/ICryptonoteProtocolObserver.h"
#include "cryptonote_core/ICore.h"
#include "cryptonote_core/ICoreObserver.h"
#include "common/ObserverManager.h"

#include <thread>
#include <boost/asio.hpp>

namespace cryptonote {
class core;
}

namespace CryptoNote {

class InProcessNode : public INode, public cryptonote::ICryptonoteProtocolObserver, public cryptonote::ICoreObserver {
public:
  InProcessNode(cryptonote::ICore& core, cryptonote::ICryptonoteProtocolQuery& protocol);

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
  virtual uint64_t getLastLocalBlockTimestamp() const override;

  virtual void getNewBlocks(std::list<crypto::hash>&& knownBlockIds, std::list<cryptonote::block_complete_entry>& newBlocks, uint64_t& startHeight, const Callback& callback) override;
  virtual void getTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices, const Callback& callback) override;
  virtual void getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount,
      std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback) override;
  virtual void relayTransaction(const cryptonote::Transaction& transaction, const Callback& callback) override;
  virtual void queryBlocks(std::list<crypto::hash>&& knownBlockIds, uint64_t timestamp, std::list<BlockCompleteEntry>& newBlocks, uint64_t& startHeight,
      const Callback& callback) override;
  virtual void getPoolSymmetricDifference(std::vector<crypto::hash>&& known_pool_tx_ids, crypto::hash known_block_id, bool& is_bc_actual, std::vector<cryptonote::Transaction>& new_txs,
    std::vector<crypto::hash>& deleted_tx_ids, const Callback& callback) override;

private:
  virtual void peerCountUpdated(size_t count) override;
  virtual void lastKnownBlockHeightUpdated(uint64_t height) override;
  virtual void blockchainUpdated() override;
  virtual void poolUpdated() override;

  void getNewBlocksAsync(std::list<crypto::hash>& knownBlockIds, std::list<cryptonote::block_complete_entry>& newBlocks, uint64_t& startHeight, const Callback& callback);
  std::error_code doGetNewBlocks(std::list<crypto::hash>&& knownBlockIds, std::list<cryptonote::block_complete_entry>& newBlocks, uint64_t& startHeight);

  void getTransactionOutsGlobalIndicesAsync(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices, const Callback& callback);
  std::error_code doGetTransactionOutsGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices);

  void getRandomOutsByAmountsAsync(std::vector<uint64_t>& amounts, uint64_t outsCount,
      std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback);
  std::error_code doGetRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount,
      std::vector<cryptonote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result);

  void relayTransactionAsync(const cryptonote::Transaction& transaction, const Callback& callback);
  std::error_code doRelayTransaction(const cryptonote::Transaction& transaction);

  void queryBlocksAsync(std::list<crypto::hash>& knownBlockIds, uint64_t timestamp, std::list<BlockCompleteEntry>& newBlocks, uint64_t& startHeight,
      const Callback& callback);
  std::error_code doQueryBlocks(std::list<crypto::hash>&& knownBlockIds, uint64_t timestamp, std::list<BlockCompleteEntry>& newBlocks, uint64_t& startHeight);

  void getPoolSymmetricDifferenceAsync(std::vector<crypto::hash>& known_pool_tx_ids, crypto::hash known_block_id, bool& is_bc_actual, std::vector<cryptonote::Transaction>& new_txs,
    std::vector<crypto::hash>& deleted_tx_ids, const Callback& callback);

  void workerFunc();

  enum State {
    NOT_INITIALIZED,
    INITIALIZED
  };

  State state;
  cryptonote::ICore& core;
  cryptonote::ICryptonoteProtocolQuery& protocol;
  tools::ObserverManager<INodeObserver> observerManager;

  boost::asio::io_service ioService;
  std::unique_ptr<std::thread> workerThread;
  std::unique_ptr<boost::asio::io_service::work> work;

  mutable std::mutex mutex;
};

} //namespace CryptoNote



