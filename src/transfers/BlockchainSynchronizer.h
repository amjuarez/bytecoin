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

  BlockchainSynchronizer(INode& node, const crypto::hash& genesisBlockHash);
  ~BlockchainSynchronizer();

  // IBlockchainSynchronizer
  virtual void addConsumer(IBlockchainConsumer* consumer) override;
  virtual bool removeConsumer(IBlockchainConsumer* consumer) override;
  virtual IStreamSerializable* getConsumerState(IBlockchainConsumer* consumer) override;

  virtual void start() override;
  virtual void stop() override;

  // IStreamSerializable
  virtual void save(std::ostream& os) override;
  virtual void load(std::istream& in) override;

  // INodeObserver
  virtual void lastKnownBlockHeightUpdated(uint64_t height) override;
  virtual void poolChanged() override;

private:

  struct GetBlocksResponse {
    uint64_t startHeight;
    std::list<BlockCompleteEntry> newBlocks;
  };

  struct GetBlocksRequest {
    GetBlocksRequest() {
      syncStart.timestamp = 0;
      syncStart.height = 0;
    }
    SynchronizationStart syncStart;
    std::list<crypto::hash> knownBlocks;
  };

  struct GetPoolResponse {
    bool isLastKnownBlockActual;
    std::vector<cryptonote::Transaction> newTxs;
    std::vector<crypto::hash> deletedTxIds;
  };

  struct GetPoolRequest {
    std::vector<crypto::hash> knownTxIds;
    crypto::hash lastKnownBlock;
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
    errorOccured = 2
  };

  //void startSync();
  void startPoolSync();
  void startBlockchainSync();

  void onGetBlocksCompleted(std::error_code ec);
  void processBlocks(GetBlocksResponse& response);
  UpdateConsumersResult updateConsumers(const BlockchainInterval& interval, const std::vector<CompleteBlock>& blocks);
  void onGetPoolChanges(std::error_code ec);
  std::error_code processPoolTxs(GetPoolResponse& response);
  
  ///second parameter is used only in case of errors returned into callback from INode, such as aborted or connection lost
  bool setFutureState(State s); 
  bool setFutureStateIf(State s, std::function<bool(void)>&& pred);

  void actualizeFutureState();
  bool checkIfShouldStop();
  bool checkIfStopped();

  void workingProcedure();

  GetBlocksRequest getCommonHistory();
  GetPoolRequest getUnionPoolHistory();
  GetPoolRequest getIntersectedPoolHistory();

  typedef std::map<IBlockchainConsumer*, std::shared_ptr<SynchronizationState>> ConsumersMap;

  ConsumersMap m_consumers;
  INode& m_node;
  const crypto::hash m_genesisBlockHash;

  std::vector<crypto::hash> knownTxIds;
  crypto::hash lastBlockId;

  State m_currentState;
  State m_futureState;
  std::unique_ptr<std::thread> workingThread;

  std::future<std::error_code> asyncOperationWaitFuture;
  std::promise<std::error_code> asyncOperationCompleted;

  std::mutex m_consumersMutex;
  std::mutex m_stateMutex;

  bool shouldSyncConsumersPool;
};

}
