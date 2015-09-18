// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <system_error>

#include "ITransaction.h"
#include "ITransfersContainer.h"
#include "IStreamSerializable.h"

namespace CryptoNote {

struct SynchronizationStart {
  uint64_t timestamp;
  uint64_t height;
};

struct AccountSubscription {
  AccountKeys keys;
  SynchronizationStart syncStart;
  size_t transactionSpendableAge;
};

class ITransfersSubscription;

class ITransfersObserver {
public:
  virtual void onError(ITransfersSubscription* object,
    uint32_t height, std::error_code ec) {
  }

  virtual void onTransactionUpdated(ITransfersSubscription* object, const Crypto::Hash& transactionHash) {}

  /**
   * \note The sender must guarantee that onTransactionDeleted() is called only after onTransactionUpdated() is called
   * for the same \a transactionHash.
   */
  virtual void onTransactionDeleted(ITransfersSubscription* object, const Crypto::Hash& transactionHash) {}
};

class ITransfersSubscription : public IObservable < ITransfersObserver > {
public:
  virtual ~ITransfersSubscription() {}

  virtual AccountPublicAddress getAddress() = 0;
  virtual ITransfersContainer& getContainer() = 0;
};

class ITransfersSynchronizer : public IStreamSerializable {
public:
  virtual ~ITransfersSynchronizer() {}

  virtual ITransfersSubscription& addSubscription(const AccountSubscription& acc) = 0;
  virtual bool removeSubscription(const AccountPublicAddress& acc) = 0;
  virtual void getSubscriptions(std::vector<AccountPublicAddress>& subscriptions) = 0;
  // returns nullptr if address is not found
  virtual ITransfersSubscription* getSubscription(const AccountPublicAddress& acc) = 0;
};

}
