// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "INode.h"
#include "SynchronizationState.h"
#include "IBlockchainSynchronizer.h"
#include "IObservableImpl.h"
#include "IStreamSerializable.h"

#include <condition_variable>
#include <mutex>
#include <atomic>
#include <future>

namespace CryptoNote {

class BlockchainSynchronizer :
  public IObservableImpl<IBlockchainSynchronizerObserver, IBlockchainSynchronizer>,
  public INodeObserver {
public:

  BlockchainSynchronizer(INode& node, const Crypto::Hash& genesisBlockHash);
  ~BlockchainSynchronizer();

  // IBlockchainSynchronizer
  virtual void addConsumer(IBlockchainConsumer* consumer) override;
  virtual bool removeConsumer(IBlockchainConsumer* consumer) override;
  virtual IStreamSerializable* getConsumerState(IBlockchainConsumer* consumer) const override;
  virtual std::vector<Crypto::Hash> getConsumerKnownBlocks(IBlockchainConsumer& consumer) const override;

  virtual std::future<std::error_code> addUnconfirmedTransaction(const ITransactionReader& transaction) override;
  virtual std::future<void> removeUnconfirmedTransaction(const Crypto::Hash& transactionHash) override;

  virtual void start() override;
  virtual void stop() override;

  // IStreamSerializable
  virtual void save(std::ostream& os) override;
  virtual void load(std::istream& in) override;

  // INodeObserver
  virtual void localBlockchainUpdated(uint32_t height) override;
  virtual void lastKnownBlockHeightUpdated(uint32_t height) override;
  virtual void poolChanged() override;

private:

  struct GetBlocksResponse {
    uint32_t startHeight;
    std::vector<BlockShortEntry> newBlocks;
  };

  struct GetBlocksRequest {
    GetBlocksRequest() {
      syncStart.timestamp = 0;
      syncStart.height = 0;
    }
    SynchronizationStart syncStart;
    std::vector<Crypto::Hash> knownBlocks;
  };

  struct GetPoolResponse {
    bool isLastKnownBlockActual;
    std::vector<std::unique_ptr<ITransactionReader>> newTxs;
    std::vector<Crypto::Hash> deletedTxIds;
  };

  struct GetPoolRequest {
    std::vector<Crypto::Hash> knownTxIds;
    Crypto::Hash lastKnownBlock;
  };

  enum class State { //prioritized finite states
    idle = 0,           //DO
    poolSync = 1,       //NOT
    blockchainSync = 2, //REORDER
    stopped = 3         //!!!
  };

  enum class UpdateConsumersResult {
    nothingChanged = 0,
    addedNewBlocks = 1,
    errorOccurred = 2
  };

  //void startSync();
  void startPoolSync();
  void startBlockchainSync();

  void processBlocks(GetBlocksResponse& response);
  UpdateConsumersResult updateConsumers(const BlockchainInterval& interval, const std::vector<CompleteBlock>& blocks);
  std::error_code processPoolTxs(GetPoolResponse& response);
  std::error_code getPoolSymmetricDifferenceSync(GetPoolRequest&& request, GetPoolResponse& response);
  std::error_code doAddUnconfirmedTransaction(const ITransactionReader& transaction);
  void doRemoveUnconfirmedTransaction(const Crypto::Hash& transactionHash);

  ///second parameter is used only in case of errors returned into callback from INode, such as aborted or connection lost
  bool setFutureState(State s); 
  bool setFutureStateIf(State s, std::function<bool(void)>&& pred);

  void actualizeFutureState();
  bool checkIfShouldStop() const;
  bool checkIfStopped() const;

  void workingProcedure();

  GetBlocksRequest getCommonHistory();
  void getPoolUnionAndIntersection(std::unordered_set<Crypto::Hash>& poolUnion, std::unordered_set<Crypto::Hash>& poolIntersection) const;
  SynchronizationState* getConsumerSynchronizationState(IBlockchainConsumer* consumer) const ;

  typedef std::map<IBlockchainConsumer*, std::shared_ptr<SynchronizationState>> ConsumersMap;

  ConsumersMap m_consumers;
  INode& m_node;
  const Crypto::Hash m_genesisBlockHash;

  Crypto::Hash lastBlockId;

  State m_currentState;
  State m_futureState;
  std::unique_ptr<std::thread> workingThread;
  std::list<std::pair<const ITransactionReader*, std::promise<std::error_code>>> m_addTransactionTasks;
  std::list<std::pair<const Crypto::Hash*, std::promise<void>>> m_removeTransactionTasks;

  mutable std::mutex m_consumersMutex;
  mutable std::mutex m_stateMutex;
  std::condition_variable m_hasWork;
};

}
