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

#include "BlockchainSynchronizer.h"

#include <functional>
#include <iostream>
#include <sstream>
#include <unordered_set>

#include "CryptoNoteCore/TransactionApi.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"

using namespace Crypto;

namespace {

inline std::vector<uint8_t> stringToVector(const std::string& s) {
  std::vector<uint8_t> vec(
    reinterpret_cast<const uint8_t*>(s.data()),
    reinterpret_cast<const uint8_t*>(s.data()) + s.size());
  return vec;
}

}

namespace CryptoNote {

BlockchainSynchronizer::BlockchainSynchronizer(INode& node, const Hash& genesisBlockHash) :
m_node(node), m_genesisBlockHash(genesisBlockHash), m_currentState(State::stopped), m_futureState(State::stopped), shouldSyncConsumersPool(true) {
}

BlockchainSynchronizer::~BlockchainSynchronizer() {
  stop();
}

void BlockchainSynchronizer::addConsumer(IBlockchainConsumer* consumer) {
  assert(consumer != nullptr);
  assert(m_consumers.count(consumer) == 0);

  if (!(checkIfStopped() && checkIfShouldStop())) {
    throw std::runtime_error("Can't add consumer, because BlockchainSynchronizer isn't stopped");
  }

  m_consumers.insert(std::make_pair(consumer, std::make_shared<SynchronizationState>(m_genesisBlockHash)));
  shouldSyncConsumersPool = true;
}

bool BlockchainSynchronizer::removeConsumer(IBlockchainConsumer* consumer) {
  assert(consumer != nullptr);

  if (!(checkIfStopped() && checkIfShouldStop())) {
    throw std::runtime_error("Can't remove consumer, because BlockchainSynchronizer isn't stopped");
  }

  return m_consumers.erase(consumer) > 0;
}

IStreamSerializable* BlockchainSynchronizer::getConsumerState(IBlockchainConsumer* consumer) {
  assert(consumer != nullptr);

  if (!(checkIfStopped() && checkIfShouldStop())) {
    throw std::runtime_error("Can't get consumer state, because BlockchainSynchronizer isn't stopped");
  }

  std::unique_lock<std::mutex> lk(m_consumersMutex);

  auto it = m_consumers.find(consumer);
  if (it == m_consumers.end()) {
    return nullptr;
  }

  return it->second.get();
}


void BlockchainSynchronizer::save(std::ostream& os) {
  os.write(reinterpret_cast<const char*>(&m_genesisBlockHash), sizeof(m_genesisBlockHash));
}

void BlockchainSynchronizer::load(std::istream& in) {
  Hash genesisBlockHash;
  in.read(reinterpret_cast<char*>(&genesisBlockHash), sizeof(genesisBlockHash));
  if (genesisBlockHash != m_genesisBlockHash) {
    throw std::runtime_error("Genesis block hash does not match stored state");
  }
}

//--------------------------- FSM ------------------------------------

bool BlockchainSynchronizer::setFutureState(State s) {
  return setFutureStateIf(s, std::bind(
    [](State futureState, State s) -> bool {
    return s > futureState;
  }, std::ref(m_futureState), s));
}

bool BlockchainSynchronizer::setFutureStateIf(State s, std::function<bool(void)>&& pred) {
  std::unique_lock<std::mutex> lk(m_stateMutex);
  if (pred()) {
    m_futureState = s;
    return true;
  }

  return false;
}

void BlockchainSynchronizer::actualizeFutureState() {
  std::unique_lock<std::mutex> lk(m_stateMutex);
  if (m_currentState == State::stopped && m_futureState == State::blockchainSync) { // start(), immideately attach observer
    m_node.addObserver(this);
  }

  if (m_futureState == State::stopped && m_currentState != State::stopped) { // stop(), immideately detach observer
    m_node.removeObserver(this);
  }

  m_currentState = m_futureState;
  switch (m_futureState) {
  case State::stopped:
    m_futureState = State::stopped;
    break;
  case State::blockchainSync:
    m_futureState = State::poolSync;
    lk.unlock();
    startBlockchainSync();
    break;
  case State::poolSync:
    m_futureState = State::idle;
    lk.unlock();
    startPoolSync();
    break;
  case State::idle:
    m_futureState = State::idle;
    lk.unlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    break;
  default:
    break;
  }
}

bool BlockchainSynchronizer::checkIfShouldStop() {
  std::unique_lock<std::mutex> lk(m_stateMutex);
  return m_futureState == State::stopped;
}

bool BlockchainSynchronizer::checkIfStopped() {
  std::unique_lock<std::mutex> lk(m_stateMutex);
  return m_currentState == State::stopped;
}


void BlockchainSynchronizer::workingProcedure() {
  while (!checkIfShouldStop()) {
    actualizeFutureState();
  }

  actualizeFutureState();
}

void BlockchainSynchronizer::start() {
  if (m_consumers.empty()) {
    throw std::runtime_error("Can't start, because BlockchainSynchronizer has no consumers");
  }

  if (!setFutureStateIf(State::blockchainSync, std::bind(
    [](State currentState, State futureState) -> bool {
    return currentState == State::stopped && futureState == State::stopped;
  }, std::ref(m_currentState), std::ref(m_futureState)))) {
    throw std::runtime_error("BlockchainSynchronizer already started");
  }

  shouldSyncConsumersPool = true;

  workingThread.reset(new std::thread([this] {this->workingProcedure(); }));
}

void BlockchainSynchronizer::stop() {
  setFutureState(State::stopped);

  // wait for previous processing to end
  if (workingThread.get() != nullptr && workingThread->joinable()) {
    workingThread->join();
  }

  workingThread.reset();
}

void BlockchainSynchronizer::lastKnownBlockHeightUpdated(uint32_t height) {
  setFutureState(State::blockchainSync);
}

void BlockchainSynchronizer::poolChanged() {
  setFutureState(State::poolSync);
}
//--------------------------- FSM END ------------------------------------

BlockchainSynchronizer::GetPoolRequest BlockchainSynchronizer::getUnionPoolHistory() {
  GetPoolRequest request;
  std::unordered_set<Hash> unionHistory;
  {
    std::unique_lock<std::mutex> lk(m_consumersMutex);
    for (auto& consumer : m_consumers) {
      std::vector<Hash> consumerKnownIds;
      consumer.first->getKnownPoolTxIds(consumerKnownIds);
      for (auto& txId : consumerKnownIds) {
        unionHistory.insert(txId);
      }
    }
  }

  for (auto& id : unionHistory) {
    request.knownTxIds.push_back(id);
  }

  request.lastKnownBlock = lastBlockId;
  return request;
}

BlockchainSynchronizer::GetPoolRequest BlockchainSynchronizer::getIntersectedPoolHistory() {
  GetPoolRequest request;
  {
    std::unique_lock<std::mutex> lk(m_consumersMutex);
    auto it = m_consumers.begin();

    it->first->getKnownPoolTxIds(request.knownTxIds);
    ++it;

    for (; it != m_consumers.end(); ++it) { //iterate over consumers
      std::vector<Hash> consumerKnownIds;
      it->first->getKnownPoolTxIds(consumerKnownIds);
      for (auto itReq = request.knownTxIds.begin(); itReq != request.knownTxIds.end();) { //iterate over intersection
        if (std::count(consumerKnownIds.begin(), consumerKnownIds.end(), *itReq) == 0) { //consumer doesn't contain id from intersection, so delete this id from intersection
          itReq = request.knownTxIds.erase(itReq);
        } else {
          ++itReq;
        }
      }
    }
  }

  request.lastKnownBlock = lastBlockId;
  return request;
}

BlockchainSynchronizer::GetBlocksRequest BlockchainSynchronizer::getCommonHistory() {
  GetBlocksRequest request;
  std::unique_lock<std::mutex> lk(m_consumersMutex);
  if (m_consumers.empty()) {
    return request;
  }

  auto shortest = m_consumers.begin();
  auto syncStart = shortest->first->getSyncStart();
  auto it = shortest;
  ++it;
  for (; it != m_consumers.end(); ++it) {
    if (it->second->getHeight() < shortest->second->getHeight()) {
      shortest = it;
    }

    auto consumerStart = it->first->getSyncStart();
    syncStart.timestamp = std::min(syncStart.timestamp, consumerStart.timestamp);
    syncStart.height = std::min(syncStart.height, consumerStart.height);
  }

  request.knownBlocks = shortest->second->getShortHistory(m_node.getLastLocalBlockHeight());
  request.syncStart = syncStart;
  return request;
}

void BlockchainSynchronizer::startBlockchainSync() {
  GetBlocksResponse response;
  GetBlocksRequest req = getCommonHistory();

  try {
    if (!req.knownBlocks.empty()) {
      asyncOperationCompleted = std::promise<std::error_code>();
      asyncOperationWaitFuture = asyncOperationCompleted.get_future();

      m_node.queryBlocks
              (std::move(req.knownBlocks), req.syncStart.timestamp, response.newBlocks, response.startHeight,
        std::bind(&BlockchainSynchronizer::onGetBlocksCompleted, this, std::placeholders::_1));

      std::error_code ec = asyncOperationWaitFuture.get();

      if (ec) {
        setFutureStateIf(State::idle, std::bind(
          [](State futureState) -> bool {
            return futureState != State::stopped; 
          }, std::ref(m_futureState)));
        m_observerManager.notify(&IBlockchainSynchronizerObserver::synchronizationCompleted, ec);
      } else {
        processBlocks(response);
      }
    }
  } catch (std::exception& e) {
    std::cout << e.what() << std::endl;
    setFutureStateIf(State::idle, std::bind(
      [](State futureState) -> bool {
      return futureState != State::stopped;
    }, std::ref(m_futureState)));
    m_observerManager.notify(
      &IBlockchainSynchronizerObserver::synchronizationCompleted,
      std::make_error_code(std::errc::invalid_argument));
  }
}

void BlockchainSynchronizer::onGetBlocksCompleted(std::error_code ec) {
  decltype(asyncOperationCompleted) detachedPromise = std::move(asyncOperationCompleted);
  detachedPromise.set_value(ec);
}

void BlockchainSynchronizer::processBlocks(GetBlocksResponse& response) {
  BlockchainInterval interval;
  interval.startHeight = response.startHeight;
  std::vector<CompleteBlock> blocks;

  for (auto& block : response.newBlocks) {
    if (checkIfShouldStop()) {
      break;
    }
    CompleteBlock completeBlock;
    completeBlock.blockHash = block.blockHash;
    interval.blocks.push_back(completeBlock.blockHash);
    if (block.hasBlock) {
      completeBlock.block = std::move(block.block);
      completeBlock.transactions.push_back(createTransactionPrefix(completeBlock.block->baseTransaction));

      try {
        for (const auto& txShortInfo : block.txsShortInfo) {
          completeBlock.transactions.push_back(createTransactionPrefix(txShortInfo.txPrefix,
                  reinterpret_cast<const Hash&>(txShortInfo.txId)));
        }
      } catch (std::exception&) {
        setFutureStateIf(State::idle, std::bind(
          [](State futureState) -> bool {
            return futureState != State::stopped;
          }, std::ref(m_futureState)));

        m_observerManager.notify(
          &IBlockchainSynchronizerObserver::synchronizationCompleted,
          std::make_error_code(std::errc::invalid_argument));

        return;
      }
    }

    blocks.push_back(std::move(completeBlock));
  }

  uint32_t processedBlockCount = response.startHeight + static_cast<uint32_t>(response.newBlocks.size());
  if (!checkIfShouldStop()) {
    response.newBlocks.clear();
    std::unique_lock<std::mutex> lk(m_consumersMutex);
    auto result = updateConsumers(interval, blocks);
    lk.unlock();

    switch (result) {
    case UpdateConsumersResult::errorOccured:
      if (setFutureStateIf(State::idle, std::bind(
        [](State futureState) -> bool {
        return futureState != State::stopped;
      }, std::ref(m_futureState)))) {
        m_observerManager.notify(
          &IBlockchainSynchronizerObserver::synchronizationCompleted,
          std::make_error_code(std::errc::invalid_argument));
      }

      break;
    case UpdateConsumersResult::nothingChanged:
      if (m_node.getLastKnownBlockHeight() != m_node.getLastLocalBlockHeight()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      } else {
        break;
      }
    case UpdateConsumersResult::addedNewBlocks:
      setFutureState(State::blockchainSync);
      m_observerManager.notify(
        &IBlockchainSynchronizerObserver::synchronizationProgressUpdated,
        processedBlockCount,
        std::max(m_node.getKnownBlockCount(), m_node.getLocalBlockCount()));
      break;
    }

    if (!blocks.empty()) {
      lastBlockId = blocks.back().blockHash;
    }
  }

  if (checkIfShouldStop()) { //Sic!
    m_observerManager.notify(
      &IBlockchainSynchronizerObserver::synchronizationCompleted,
      std::make_error_code(std::errc::interrupted));
  }
}

BlockchainSynchronizer::UpdateConsumersResult BlockchainSynchronizer::updateConsumers(const BlockchainInterval& interval, const std::vector<CompleteBlock>& blocks) {
  bool smthChanged = false;

  for (auto& kv : m_consumers) {
    auto result = kv.second->checkInterval(interval);

    if (result.detachRequired) {
      kv.first->onBlockchainDetach(result.detachHeight);
      kv.second->detach(result.detachHeight);
    }

    if (result.hasNewBlocks) {
      uint32_t startOffset = result.newBlockHeight - interval.startHeight;
      // update consumer
      if (kv.first->onNewBlocks(
        blocks.data() + startOffset,
        result.newBlockHeight,
        static_cast<uint32_t>(blocks.size()) - startOffset)) {
        // update state if consumer succeeded
        kv.second->addBlocks(
          interval.blocks.data() + startOffset,
          result.newBlockHeight,
          static_cast<uint32_t>(interval.blocks.size()) - startOffset);
        smthChanged = true;
      } else {
        return UpdateConsumersResult::errorOccured;
      }
    }
  }

  return smthChanged ? UpdateConsumersResult::addedNewBlocks : UpdateConsumersResult::nothingChanged;
}

void BlockchainSynchronizer::startPoolSync() {
  GetPoolResponse unionResponse;
  GetPoolRequest unionRequest = getUnionPoolHistory();

  asyncOperationCompleted = std::promise<std::error_code>();
  asyncOperationWaitFuture = asyncOperationCompleted.get_future();

  unionResponse.isLastKnownBlockActual = false;

  m_node.getPoolSymmetricDifference(std::move(unionRequest.knownTxIds), std::move(unionRequest.lastKnownBlock), unionResponse.isLastKnownBlockActual,
    unionResponse.newTxs, unionResponse.deletedTxIds, std::bind(&BlockchainSynchronizer::onGetPoolChanges, this, std::placeholders::_1));

  std::error_code ec = asyncOperationWaitFuture.get();

  if (ec) {
    setFutureStateIf(State::idle, std::bind(
      [](State futureState) -> bool {
      return futureState != State::stopped;
    }, std::ref(m_futureState)));
    m_observerManager.notify(
      &IBlockchainSynchronizerObserver::synchronizationCompleted,
      ec);
  } else { //get union ok
    if (!unionResponse.isLastKnownBlockActual) { //bc outdated
      setFutureState(State::blockchainSync);
    } else {
      if (!shouldSyncConsumersPool) { //usual case, start pool processing
        m_observerManager.notify(
          &IBlockchainSynchronizerObserver::synchronizationCompleted,
          processPoolTxs(unionResponse));
      } else {// first launch, we should sync consumers' pools, so let's ask for intersection
        GetPoolResponse intersectionResponse;
        GetPoolRequest intersectionRequest = getIntersectedPoolHistory();

        asyncOperationCompleted = std::promise<std::error_code>();
        asyncOperationWaitFuture = asyncOperationCompleted.get_future();

        intersectionResponse.isLastKnownBlockActual = false;

        m_node.getPoolSymmetricDifference(std::move(intersectionRequest.knownTxIds), std::move(intersectionRequest.lastKnownBlock), intersectionResponse.isLastKnownBlockActual,
          intersectionResponse.newTxs, intersectionResponse.deletedTxIds, std::bind(&BlockchainSynchronizer::onGetPoolChanges, this, std::placeholders::_1));

        std::error_code ec2 = asyncOperationWaitFuture.get();

        if (ec2) {
          setFutureStateIf(State::idle, std::bind(
            [](State futureState) -> bool {
            return futureState != State::stopped;
          }, std::ref(m_futureState)));
          m_observerManager.notify(
            &IBlockchainSynchronizerObserver::synchronizationCompleted,
            ec2);
        } else { //get intersection ok
          if (!intersectionResponse.isLastKnownBlockActual) { //bc outdated
            setFutureState(State::blockchainSync);
          } else {
            intersectionResponse.deletedTxIds.assign(unionResponse.deletedTxIds.begin(), unionResponse.deletedTxIds.end());
            std::error_code ec3 = processPoolTxs(intersectionResponse);

            //notify about error, or success
            m_observerManager.notify(
              &IBlockchainSynchronizerObserver::synchronizationCompleted,
              ec3);

            if (!ec3) {
              shouldSyncConsumersPool = false;
            }
          }
        }
      }
    }
  }
}

void BlockchainSynchronizer::onGetPoolChanges(std::error_code ec) {
  decltype(asyncOperationCompleted) detachedPromise = std::move(asyncOperationCompleted);
  detachedPromise.set_value(ec);
}

std::error_code BlockchainSynchronizer::processPoolTxs(GetPoolResponse& response) {
  std::error_code error;
  {
    std::unique_lock<std::mutex> lk(m_consumersMutex);
    for (auto& consumer : m_consumers) {
      if (checkIfShouldStop()) { //if stop, return immediately, without notification
        return std::make_error_code(std::errc::interrupted);
      }

      error = consumer.first->onPoolUpdated(response.newTxs, response.deletedTxIds);
      if (error) {
        break;
      }
    }
  }

  return error;
}

}
