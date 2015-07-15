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

#include "BlockchainExplorer.h"

#include <future>
#include <functional>
#include <memory>

#include "cryptonote_core/cryptonote_format_utils.h"
#include "BlockchainExplorerErrors.h"

using namespace Logging;

namespace CryptoNote {

class NodeRequest {
public:

  NodeRequest(const std::function<void(const INode::Callback&)>& request) : requestFunc(request) {}

  std::error_code performBlocking() {
    requestFunc(std::bind(&NodeRequest::completeionCallback, this, std::placeholders::_1));
    return promise.get_future().get();
  }

  void performAsync(const INode::Callback& callback) {
    requestFunc(callback);
  }

private:
  void completeionCallback(std::error_code ec) {
    promise.set_value(ec);
  }

  std::promise<std::error_code> promise;
  const std::function<void(const INode::Callback&)> requestFunc;
};

BlockchainExplorer::BlockchainExplorer(INode& node, Logging::ILogger& logger) : node(node), logger(logger, "BlockchainExplorer"), state(NOT_INITIALIZED) {}

BlockchainExplorer::~BlockchainExplorer() {}
    
bool BlockchainExplorer::addObserver(IBlockchainObserver* observer) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  return observerManager.add(observer);
}

bool BlockchainExplorer::removeObserver(IBlockchainObserver* observer) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  return observerManager.remove(observer);
}

void BlockchainExplorer::init() {
  if (state.load() != NOT_INITIALIZED) {
    logger(ERROR) << "Init called on already initialized BlockchainExplorer.";
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::ALREADY_INITIALIZED));
  }
  if (node.addObserver(this)) {
    state.store(INITIALIZED);
  } else {
    logger(ERROR) << "Can't add observer to node.";
    state.store(NOT_INITIALIZED);
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::INTERNAL_ERROR));
  }
  if (getBlockchainTop(knownBlockchainTop)) {
    knownBlockchainTopHeight = knownBlockchainTop.height;
  } else {
    logger(ERROR) << "Can't get blockchain top.";
    state.store(NOT_INITIALIZED);
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::INTERNAL_ERROR));
  }
}

void BlockchainExplorer::shutdown() {
  if (state.load() != INITIALIZED) {
    logger(ERROR) << "Shutdown called on not initialized BlockchainExplorer.";
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }
  node.removeObserver(this);
  state.store(NOT_INITIALIZED);
}

bool BlockchainExplorer::getBlocks(const std::vector<uint64_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  logger(DEBUGGING) << "Get blocks by height request came.";
  NodeRequest request(
    std::bind(
      static_cast<
        void(INode::*)(
          const std::vector<uint64_t>&, 
          std::vector<std::vector<BlockDetails>>&, 
          const INode::Callback&
        )
      >(&INode::getBlocks), 
      std::ref(node), 
      std::cref(blockHeights), 
      std::ref(blocks),
      std::placeholders::_1
    )
  );
  std::error_code ec = request.performBlocking();
  if (ec) {
    logger(ERROR) << "Can't get blocks by height: " << ec.message();
    throw std::system_error(ec);
  }
  assert(blocks.size() == blockHeights.size());
  return true;
}

bool BlockchainExplorer::getBlocks(const std::vector<std::array<uint8_t, 32>>& blockHashes, std::vector<BlockDetails>& blocks) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  logger(DEBUGGING) << "Get blocks by hash request came.";
  NodeRequest request(
    std::bind(
      static_cast<
        void(INode::*)(
          const std::vector<crypto::hash>&, 
          std::vector<BlockDetails>&, 
          const INode::Callback&
        )
      >(&INode::getBlocks), 
      std::ref(node), 
      std::cref(reinterpret_cast<const std::vector<crypto::hash>&>(blockHashes)), 
      std::ref(blocks),
      std::placeholders::_1
    )
  );
  std::error_code ec = request.performBlocking();
  if (ec) {
    logger(ERROR) << "Can't get blocks by hash: " << ec.message();
    throw std::system_error(ec);
  }
  assert(blocks.size() == blockHashes.size());
  return true;
}

bool BlockchainExplorer::getBlockchainTop(BlockDetails& topBlock) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  logger(DEBUGGING) << "Get blockchain top request came.";
  uint64_t lastHeight = node.getLastLocalBlockHeight();

  std::vector<uint64_t> heights;
  heights.push_back(std::move(lastHeight));

  std::vector<std::vector<BlockDetails>> blocks;
  if (!getBlocks(heights, blocks)) {
    logger(ERROR) << "Can't get blockchain top.";
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::INTERNAL_ERROR));
  }
  assert(blocks.size() == heights.size() && blocks.size() == 1);

  bool gotMainchainBlock = false;
  for (const BlockDetails& block : blocks.back()) {
    if (!block.isOrphaned) {
      topBlock = block;
      gotMainchainBlock = true;
      break;
    }
  }

  if (!gotMainchainBlock) {
    logger(ERROR) << "Can't get blockchain top: all blocks on height " << lastHeight << " are orphaned.";
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::INTERNAL_ERROR));
  }
  return true;
}

bool BlockchainExplorer::getTransactions(const std::vector<std::array<uint8_t, 32>>& transactionHashes, std::vector<TransactionDetails>& transactions) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  logger(DEBUGGING) << "Get transactions request came.";
  NodeRequest request(
    std::bind(
      &INode::getTransactions, 
      std::ref(node), 
      std::cref(reinterpret_cast<const std::vector<crypto::hash>&>(transactionHashes)), 
      std::ref(transactions),
      std::placeholders::_1
    )
  );
  std::error_code ec = request.performBlocking();
  if (ec) {
    logger(ERROR) << "Can't get transactions: " << ec.message();
    throw std::system_error(ec);
  }
  return true;
}

bool BlockchainExplorer::getPoolState(const std::vector<std::array<uint8_t, 32>>& knownPoolTransactionHashes, std::array<uint8_t, 32> knownBlockchainTopHash, bool& isBlockchainActual, std::vector<TransactionDetails>& newTransactions, std::vector<std::array<uint8_t, 32>>& removedTransactions) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  logger(DEBUGGING) << "Get pool state request came.";
  std::vector<Transaction> rawNewTransactions;

  NodeRequest request(
    [&](const INode::Callback& callback) {
      std::vector<crypto::hash> hashes;
      for (const std::array<uint8_t, 32>& hash : knownPoolTransactionHashes) {
        hashes.push_back(std::move(reinterpret_cast<const crypto::hash&>(hash)));
      }
      node.getPoolSymmetricDifference(
        std::move(hashes),
        reinterpret_cast<crypto::hash&>(knownBlockchainTopHash),
        isBlockchainActual,
        rawNewTransactions,
        reinterpret_cast<std::vector<crypto::hash>&>(removedTransactions),
        callback
      );
    }
  );
  std::error_code ec = request.performBlocking();
  if (ec) {
    logger(ERROR) << "Can't get pool state: " << ec.message();
    throw std::system_error(ec);
  }

  std::vector<std::array<uint8_t, 32>> newTransactionsHashes;
  for (const Transaction& rawTransaction : rawNewTransactions) {
    crypto::hash transactionHash = get_transaction_hash(rawTransaction);
    newTransactionsHashes.push_back(std::move(reinterpret_cast<const std::array<uint8_t, 32>&>(transactionHash)));
  }

  return getTransactions(newTransactionsHashes, newTransactions);
}

uint64_t BlockchainExplorer::getRewardBlocksWindow() {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }
  return parameters::CRYPTONOTE_REWARD_BLOCKS_WINDOW;
}

uint64_t BlockchainExplorer::getFullRewardMaxBlockSize(uint8_t majorVersion) {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }
  if (majorVersion > 1) {
    return parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE;
  } else {
    return parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1;
  }
}

bool BlockchainExplorer::isSynchronized() {
  if (state.load() != INITIALIZED) {
    throw std::system_error(make_error_code(CryptoNote::error::BlockchainExplorerErrorCodes::NOT_INITIALIZED));
  }

  logger(DEBUGGING) << "Synchronization status request came.";
  bool syncStatus = false;
  NodeRequest request(
    std::bind(
      &INode::isSynchronized, 
      std::ref(node), 
      std::ref(syncStatus),
      std::placeholders::_1
    )
  );
  std::error_code ec = request.performBlocking();
  if (ec) {
    logger(ERROR) << "Can't get synchronization status: " << ec.message();
    throw std::system_error(ec);
  }
  return syncStatus;
}

void BlockchainExplorer::poolChanged() {
  logger(DEBUGGING) << "Got poolChanged notification.";

  std::unique_lock<std::mutex> lock(mutex);

  std::shared_ptr<std::vector<Transaction>> rawNewTransactionsPtr = std::make_shared<std::vector<Transaction>>();
  std::shared_ptr<std::vector<crypto::hash>> removedTransactionsPtr = std::make_shared<std::vector<crypto::hash>>();
  std::shared_ptr<bool> isBlockchainActualPtr = std::make_shared<bool>(false);

  NodeRequest request(
    [this, rawNewTransactionsPtr, removedTransactionsPtr, isBlockchainActualPtr](const INode::Callback& callback) {
      std::vector<crypto::hash> hashes;
      for (const crypto::hash& hash : knownPoolState) {
        hashes.push_back(std::move(hash));
      }
      node.getPoolSymmetricDifference(
        std::move(hashes),
        reinterpret_cast<crypto::hash&>(knownBlockchainTop.hash),
        *isBlockchainActualPtr,
        *rawNewTransactionsPtr,
        *removedTransactionsPtr,
        callback
      );
    }
  );
  request.performAsync(
    [this, rawNewTransactionsPtr, removedTransactionsPtr, isBlockchainActualPtr](std::error_code ec) {
      if (ec) {
        logger(ERROR) << "Can't send poolChanged notification because can't get pool symmetric difference: " << ec.message();
        return;
      }

      if (!*isBlockchainActualPtr) {
        logger(WARNING) << "Blockchain not actual.";
      }

      std::unique_lock<std::mutex> lock(mutex);

      std::shared_ptr<std::vector<crypto::hash>> newTransactionsHashesPtr = std::make_shared<std::vector<crypto::hash>>();
      for (const Transaction& rawTransaction : *rawNewTransactionsPtr) {
        crypto::hash transactionHash = get_transaction_hash(rawTransaction);
        bool inserted = knownPoolState.emplace(transactionHash).second;
        if (inserted) {
          newTransactionsHashesPtr->push_back(std::move(transactionHash));
        }
      }
      
      std::shared_ptr<std::vector<std::pair<std::array<uint8_t, 32>, TransactionRemoveReason>>> removedTransactionsHashesPtr = std::make_shared<std::vector<std::pair<std::array<uint8_t, 32>, TransactionRemoveReason>>>();
      for (const crypto::hash hash : *removedTransactionsPtr) {
        auto iter = knownPoolState.find(hash);
        if (iter != knownPoolState.end()) {
          removedTransactionsHashesPtr->push_back(
            std::move(std::make_pair(
              reinterpret_cast<const std::array<uint8_t, 32>&>(hash), 
              TransactionRemoveReason::INCLUDED_IN_BLOCK  //Can't have real reason here.
            ))
          );
          knownPoolState.erase(iter);
        }
      }

      std::shared_ptr<std::vector<TransactionDetails>> newTransactionsPtr = std::make_shared<std::vector<TransactionDetails>>();
      NodeRequest request(
        std::bind(
          &INode::getTransactions, 
          std::ref(node), 
          std::cref(*newTransactionsHashesPtr), 
          std::ref(*newTransactionsPtr),
          std::placeholders::_1
        )
      );
      request.performAsync(
        [this, newTransactionsHashesPtr, newTransactionsPtr, removedTransactionsHashesPtr](std::error_code ec) {
          if (ec) {
            logger(ERROR) << "Can't send poolChanged notification because can't get transactions: " << ec.message();
            return;
          }
          if (!newTransactionsPtr->empty() || !removedTransactionsHashesPtr->empty()) {
            observerManager.notify(&IBlockchainObserver::poolUpdated, *newTransactionsPtr, *removedTransactionsHashesPtr);
            logger(DEBUGGING) << "poolUpdated notification was successfully sent.";
          }
        }
      );
    }
  );
  
}

void BlockchainExplorer::blockchainSynchronized(uint64_t topHeight) {
  logger(DEBUGGING) << "Got blockchainSynchronized notification.";

  std::shared_ptr<std::vector<uint64_t>> blockHeightsPtr = std::make_shared<std::vector<uint64_t>>();
  std::shared_ptr<std::vector<std::vector<BlockDetails>>> blocksPtr = std::make_shared<std::vector<std::vector<BlockDetails>>>();

  blockHeightsPtr->push_back(topHeight);

  NodeRequest request(
    std::bind(
      static_cast<
        void(INode::*)(
          const std::vector<uint64_t>&, 
          std::vector<std::vector<BlockDetails>>&, 
          const INode::Callback&
        )
      >(&INode::getBlocks), 
      std::ref(node), 
      std::cref(*blockHeightsPtr), 
      std::ref(*blocksPtr),
      std::placeholders::_1
    )
  );

  request.performAsync(
    [this, blockHeightsPtr, blocksPtr, topHeight](std::error_code ec) {
      if (ec) {
        logger(ERROR) << "Can't send blockchainSynchronized notification because can't get blocks by height: " << ec.message();
        return;
      }
      assert(blocksPtr->size() == blockHeightsPtr->size() && blocksPtr->size() == 1);

      BlockDetails topMainchainBlock;
      bool gotMainchainBlock = false;
      for (const BlockDetails& block : blocksPtr->back()) {
        if (!block.isOrphaned) {
          topMainchainBlock = block;
          gotMainchainBlock = true;
          break;
        }
      }

      if (!gotMainchainBlock) {
        logger(ERROR) << "Can't send blockchainSynchronized notification because can't get blockchain top: all blocks on height " << topHeight << " are orphaned.";
        return;
      }

      observerManager.notify(&IBlockchainObserver::blockchainSynchronized, topMainchainBlock);
      logger(DEBUGGING) << "blockchainSynchronized notification was successfully sent.";
    }
  );
}

void BlockchainExplorer::localBlockchainUpdated(uint64_t height) {
  logger(DEBUGGING) << "Got localBlockchainUpdated notification.";

  std::unique_lock<std::mutex> lock(mutex);

  assert(height >= knownBlockchainTopHeight);

  std::shared_ptr<std::vector<uint64_t>> blockHeightsPtr = std::make_shared<std::vector<uint64_t>>();
  std::shared_ptr<std::vector<std::vector<BlockDetails>>> blocksPtr = std::make_shared<std::vector<std::vector<BlockDetails>>>();

  for (size_t i = knownBlockchainTopHeight; i <= height; ++i) {
    blockHeightsPtr->push_back(i);
  }

  knownBlockchainTopHeight = height;

  NodeRequest request(
    std::bind(
      static_cast<
        void(INode::*)(
          const std::vector<uint64_t>&, 
          std::vector<std::vector<BlockDetails>>&, 
          const INode::Callback&
        )
      >(&INode::getBlocks), 
      std::ref(node), 
      std::cref(*blockHeightsPtr), 
      std::ref(*blocksPtr),
      std::placeholders::_1
    )
  );

  request.performAsync(
    [this, blockHeightsPtr, blocksPtr](std::error_code ec) {
      if (ec) {
        logger(ERROR) << "Can't send blockchainUpdated notification because can't get blocks by height: " << ec.message();
        return;
      }
      assert(blocksPtr->size() == blockHeightsPtr->size());

      std::unique_lock<std::mutex> lock(mutex);

      BlockDetails topMainchainBlock;
      bool gotTopMainchainBlock = false;
      uint64_t topHeight = 0;

      std::vector<BlockDetails> newBlocks;
      std::vector<BlockDetails> orphanedBlocks;
      for (const std::vector<BlockDetails>& sameHeightBlocks : *blocksPtr) {
        for (const BlockDetails& block : sameHeightBlocks) {
          if (topHeight < block.height) {
            topHeight = block.height;
            gotTopMainchainBlock = false;
          }
          if (block.isOrphaned) {
            orphanedBlocks.push_back(block);
          } else {
            if (block.height > knownBlockchainTop.height || block.hash != knownBlockchainTop.hash) {
              newBlocks.push_back(block);
            }
            if (!gotTopMainchainBlock) {
              topMainchainBlock = block;
              gotTopMainchainBlock = true;
            }
          }
        }
      }

      if (!gotTopMainchainBlock) {
        logger(ERROR) << "Can't send localBlockchainUpdated notification because can't get blockchain top: all blocks on height " << topHeight << " are orphaned.";
        return;
      }

      knownBlockchainTop = topMainchainBlock;

      observerManager.notify(&IBlockchainObserver::blockchainUpdated, newBlocks, orphanedBlocks);
      logger(DEBUGGING) << "localBlockchainUpdated notification was successfully sent.";
    }
  );
}

}
