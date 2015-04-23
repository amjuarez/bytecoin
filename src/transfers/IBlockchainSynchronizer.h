// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <system_error>

#include "crypto/crypto.h"
#include "cryptonote_core/cryptonote_basic.h"

#include "IObservable.h"
#include "IStreamSerializable.h"
#include "ITransfersSynchronizer.h"

namespace CryptoNote {

struct CompleteBlock;

class IBlockchainSynchronizerObserver {
public:
  virtual void synchronizationProgressUpdated(uint64_t current, uint64_t total) {}
  virtual void synchronizationCompleted(std::error_code result) {}
};

class IBlockchainConsumer {
public:

  virtual SynchronizationStart getSyncStart() = 0;
  virtual void getKnownPoolTxIds(std::vector<crypto::hash>& ids) = 0;
  virtual void onBlockchainDetach(uint64_t height) = 0;
  virtual bool onNewBlocks(const CompleteBlock* blocks, uint64_t startHeight, size_t count) = 0;
  virtual std::error_code onPoolUpdated(const std::vector<cryptonote::Transaction>& addedTransactions, const std::vector<crypto::hash>& deletedTransactions) = 0;
};


class IBlockchainSynchronizer :
  public IObservable<IBlockchainSynchronizerObserver>,
  public IStreamSerializable {
public:
  virtual void addConsumer(IBlockchainConsumer* consumer) = 0;
  virtual bool removeConsumer(IBlockchainConsumer* consumer) = 0;
  virtual IStreamSerializable* getConsumerState(IBlockchainConsumer* consumer) = 0;

  virtual void start() = 0;
  virtual void stop() = 0;
};

}
