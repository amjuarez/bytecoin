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
  m_node(node),
  m_genesisBlockHash(genesisBlockHash),
  m_currentState(State::stopped),
  m_futureState(State::stopped) {
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
}

bool BlockchainSynchronizer::removeConsumer(IBlockchainConsumer* consumer) {
  assert(consumer != nullptr);

  if (!(checkIfStopped() && checkIfShouldStop())) {
    throw std::runtime_error("Can't remove consumer, because BlockchainSynchronizer isn't stopped");
  }

  return m_consumers.erase(consumer) > 0;
}

IStreamSerializable* BlockchainSynchronizer::getConsumerState(IBlockchainConsumer* consumer) const {
  std::unique_lock<std::mutex> lk(m_consumersMutex);
  return getConsumerSynchronizationState(consumer);
}

std::vector<Crypto::Hash> BlockchainSynchronizer::getConsumerKnownBlocks(IBlockchainConsumer& consumer) const {
  std::unique_lock<std::mutex> lk(m_consumersMutex);

  auto state = getConsumerSynchronizationState(&consumer);
  if (state == nullptr) {
    throw std::invalid_argument("Consumer not found");
  }

  return state->getKnownBlockHashes();
}

std::future<std::error_code> BlockchainSynchronizer::addUnconfirmedTransaction(const ITransactionReader& transaction) {
  std::unique_lock<std::mutex> lock(m_stateMutex);

  if (m_currentState == State::stopped || m_futureState == State::stopped) {
    throw std::runtime_error("Can't add transaction, because BlockchainSynchronizer is stopped");
  }

  std::promise<std::error_code> promise;
  auto future = promise.get_future();
  m_addTransactionTasks.emplace_back(&transaction, std::move(promise));
  m_hasWork.notify_one();

  return future;
}

std::future<void> BlockchainSynchronizer::removeUnconfirmedTransaction(const Crypto::Hash& transactionHash) {
  std::unique_lock<std::mutex> lock(m_stateMutex);

  if (m_currentState == State::stopped || m_futureState == State::stopped) {
    throw std::runtime_error("Can't remove transaction, because BlockchainSynchronizer is stopped");
  }

  std::promise<void> promise;
  auto future = promise.get_future();
  m_removeTransactionTasks.emplace_back(&transactionHash, std::move(promise));
  m_hasWork.notify_one();

  return future;
}

std::error_code BlockchainSynchronizer::doAddUnconfirmedTransaction(const ITransactionReader& transaction) {
  std::unique_lock<std::mutex> lk(m_consumersMutex);

  std::error_code ec;
  auto addIt = m_consumers.begin();
  for (; addIt != m_consumers.end(); ++addIt) {
    ec = addIt->first->addUnconfirmedTransaction(transaction);
    if (ec) {
      break;
    }
  }

  if (ec) {
    auto transactionHash = transaction.getTransactionHash();
    for (auto rollbackIt = m_consumers.begin(); rollbackIt != addIt; ++rollbackIt) {
      rollbackIt->first->removeUnconfirmedTransaction(transactionHash);
    }
  }

  return ec;
}

void BlockchainSynchronizer::doRemoveUnconfirmedTransaction(const Crypto::Hash& transactionHash) {
  std::unique_lock<std::mutex> lk(m_consumersMutex);

  for (auto& consumer : m_consumers) {
    consumer.first->removeUnconfirmedTransaction(transactionHash);
  }
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
  return setFutureStateIf(s, [this, s] { return s > m_futureState; });
}

bool BlockchainSynchronizer::setFutureStateIf(State s, std::function<bool(void)>&& pred) {
  std::unique_lock<std::mutex> lk(m_stateMutex);
  if (pred()) {
    m_futureState = s;
    m_hasWork.notify_one();
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

  while (!m_removeTransactionTasks.empty()) {
    auto& task = m_removeTransactionTasks.front();
    const Crypto::Hash& transactionHash = *task.first;
    auto detachedPromise = std::move(task.second);
    m_removeTransactionTasks.pop_front();

    try {
      doRemoveUnconfirmedTransaction(transactionHash);
      detachedPromise.set_value();
    } catch (...) {
      detachedPromise.set_exception(std::current_exception());
    }
  }

  while (!m_addTransactionTasks.empty()) {
    auto& task = m_addTransactionTasks.front();
    const ITransactionReader& transaction = *task.first;
    auto detachedPromise = std::move(task.second);
    m_addTransactionTasks.pop_front();

    try {
      auto ec = doAddUnconfirmedTransaction(transaction);
      detachedPromise.set_value(ec);
    } catch (...) {
      detachedPromise.set_exception(std::current_exception());
    }
  }

  m_currentState = m_futureState;
  switch (m_futureState) {
  case State::stopped:
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
    m_hasWork.wait(lk, [this] {
      return m_futureState != State::idle || !m_removeTransactionTasks.empty() || !m_addTransactionTasks.empty();
    });
    lk.unlock();
    break;
  default:
    break;
  }
}

bool BlockchainSynchronizer::checkIfShouldStop() const {
  std::unique_lock<std::mutex> lk(m_stateMutex);
  return m_futureState == State::stopped;
}

bool BlockchainSynchronizer::checkIfStopped() const {
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

  if (!setFutureStateIf(State::blockchainSync, [this] { return m_currentState == State::stopped && m_futureState == State::stopped; })) {
    throw std::runtime_error("BlockchainSynchronizer already started");
  }

  workingThread.reset(new std::thread([this] { workingProcedure(); }));
}

void BlockchainSynchronizer::stop() {
  setFutureState(State::stopped);

  // wait for previous processing to end
  if (workingThread.get() != nullptr && workingThread->joinable()) {
    workingThread->join();
  }

  workingThread.reset();
}

void BlockchainSynchronizer::localBlockchainUpdated(uint32_t /*height*/) {
  setFutureState(State::blockchainSync);
}

void BlockchainSynchronizer::lastKnownBlockHeightUpdated(uint32_t /*height*/) {
  setFutureState(State::blockchainSync);
}

void BlockchainSynchronizer::poolChanged() {
  setFutureState(State::poolSync);
}
//--------------------------- FSM END ------------------------------------

void BlockchainSynchronizer::getPoolUnionAndIntersection(std::unordered_set<Crypto::Hash>& poolUnion, std::unordered_set<Crypto::Hash>& poolIntersection) const {
  std::unique_lock<std::mutex> lk(m_consumersMutex);

  auto itConsumers = m_consumers.begin();
  poolUnion = itConsumers->first->getKnownPoolTxIds();
  poolIntersection = itConsumers->first->getKnownPoolTxIds();
  ++itConsumers;

  for (; itConsumers != m_consumers.end(); ++itConsumers) {
    const std::unordered_set<Crypto::Hash>& consumerKnownIds = itConsumers->first->getKnownPoolTxIds();

    poolUnion.insert(consumerKnownIds.begin(), consumerKnownIds.end());

    for (auto itIntersection = poolIntersection.begin(); itIntersection != poolIntersection.end();) {
      if (consumerKnownIds.count(*itIntersection) == 0) {
        itIntersection = poolIntersection.erase(itIntersection);
      } else {
        ++itIntersection;
      }
    }
  }
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
      auto queryBlocksCompleted = std::promise<std::error_code>();
      auto queryBlocksWaitFuture = queryBlocksCompleted.get_future();

      m_node.queryBlocks(
        std::move(req.knownBlocks),
        req.syncStart.timestamp,
        response.newBlocks,
        response.startHeight,
        [&queryBlocksCompleted](std::error_code ec) {
          auto detachedPromise = std::move(queryBlocksCompleted);
          detachedPromise.set_value(ec);
        });

      std::error_code ec = queryBlocksWaitFuture.get();

      if (ec) {
        setFutureStateIf(State::idle, [this] { return m_futureState != State::stopped; });
        m_observerManager.notify(&IBlockchainSynchronizerObserver::synchronizationCompleted, ec);
      } else {
        processBlocks(response);
      }
    }
  } catch (std::exception&) {
    setFutureStateIf(State::idle,  [this] { return m_futureState != State::stopped; });
    m_observerManager.notify(&IBlockchainSynchronizerObserver::synchronizationCompleted, std::make_error_code(std::errc::invalid_argument));
  }
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
          completeBlock.transactions.push_back(createTransactionPrefix(txShortInfo.txPrefix, reinterpret_cast<const Hash&>(txShortInfo.txId)));
        }
      } catch (std::exception&) {
        setFutureStateIf(State::idle, [this] { return m_futureState != State::stopped; });
        m_observerManager.notify(&IBlockchainSynchronizerObserver::synchronizationCompleted, std::make_error_code(std::errc::invalid_argument));
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
    case UpdateConsumersResult::errorOccurred:
      if (setFutureStateIf(State::idle, [this] { return m_futureState != State::stopped; })) {
        m_observerManager.notify(&IBlockchainSynchronizerObserver::synchronizationCompleted, std::make_error_code(std::errc::invalid_argument));
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
    m_observerManager.notify(&IBlockchainSynchronizerObserver::synchronizationCompleted, std::make_error_code(std::errc::interrupted));
  }
}

/// \pre m_consumersMutex is locked
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
      if (kv.first->onNewBlocks(blocks.data() + startOffset, result.newBlockHeight, static_cast<uint32_t>(blocks.size()) - startOffset)) {
        // update state if consumer succeeded
        kv.second->addBlocks(interval.blocks.data() + startOffset, result.newBlockHeight, static_cast<uint32_t>(interval.blocks.size()) - startOffset);
        smthChanged = true;
      } else {
        return UpdateConsumersResult::errorOccurred;
      }
    }
  }

  return smthChanged ? UpdateConsumersResult::addedNewBlocks : UpdateConsumersResult::nothingChanged;
}

void BlockchainSynchronizer::startPoolSync() {
  std::unordered_set<Crypto::Hash> unionPoolHistory;
  std::unordered_set<Crypto::Hash> intersectedPoolHistory;
  getPoolUnionAndIntersection(unionPoolHistory, intersectedPoolHistory);

  GetPoolRequest unionRequest;
  unionRequest.knownTxIds.assign(unionPoolHistory.begin(), unionPoolHistory.end());
  unionRequest.lastKnownBlock = lastBlockId;

  GetPoolResponse unionResponse;
  unionResponse.isLastKnownBlockActual = false;

  std::error_code ec = getPoolSymmetricDifferenceSync(std::move(unionRequest), unionResponse);

  if (ec) {
    setFutureStateIf(State::idle, [this] { return m_futureState != State::stopped; });
    m_observerManager.notify(&IBlockchainSynchronizerObserver::synchronizationCompleted, ec);
  } else { //get union ok
    if (!unionResponse.isLastKnownBlockActual) { //bc outdated
      setFutureState(State::blockchainSync);
    } else {
      if (unionPoolHistory == intersectedPoolHistory) { //usual case, start pool processing
        m_observerManager.notify(&IBlockchainSynchronizerObserver::synchronizationCompleted, processPoolTxs(unionResponse));
      } else {
        GetPoolRequest intersectionRequest;
        intersectionRequest.knownTxIds.assign(intersectedPoolHistory.begin(), intersectedPoolHistory.end());
        intersectionRequest.lastKnownBlock = lastBlockId;

        GetPoolResponse intersectionResponse;
        intersectionResponse.isLastKnownBlockActual = false;

        std::error_code ec2 = getPoolSymmetricDifferenceSync(std::move(intersectionRequest), intersectionResponse);

        if (ec2) {
          setFutureStateIf(State::idle, [this] { return m_futureState != State::stopped; });
          m_observerManager.notify(&IBlockchainSynchronizerObserver::synchronizationCompleted, ec2);
        } else { //get intersection ok
          if (!intersectionResponse.isLastKnownBlockActual) { //bc outdated
            setFutureState(State::blockchainSync);
          } else {
            intersectionResponse.deletedTxIds.assign(unionResponse.deletedTxIds.begin(), unionResponse.deletedTxIds.end());
            std::error_code ec3 = processPoolTxs(intersectionResponse);

            //notify about error, or success
            m_observerManager.notify(&IBlockchainSynchronizerObserver::synchronizationCompleted, ec3);
          }
        }
      }
    }
  }
}

std::error_code BlockchainSynchronizer::getPoolSymmetricDifferenceSync(GetPoolRequest&& request, GetPoolResponse& response) {
  auto promise = std::promise<std::error_code>();
  auto future = promise.get_future();

  m_node.getPoolSymmetricDifference(
    std::move(request.knownTxIds),
    std::move(request.lastKnownBlock),
    response.isLastKnownBlockActual,
    response.newTxs,
    response.deletedTxIds,
    [&promise](std::error_code ec) {
      auto detachedPromise = std::move(promise);
      detachedPromise.set_value(ec);
    });

  return future.get();
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

///pre: m_consumersMutex is locked
SynchronizationState* BlockchainSynchronizer::getConsumerSynchronizationState(IBlockchainConsumer* consumer) const {
  assert(consumer != nullptr);

  if (!(checkIfStopped() && checkIfShouldStop())) {
    throw std::runtime_error("Can't get consumer state, because BlockchainSynchronizer isn't stopped");
  }

  auto it = m_consumers.find(consumer);
  if (it == m_consumers.end()) {
    return nullptr;
  }

  return it->second.get();
}

}
