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
