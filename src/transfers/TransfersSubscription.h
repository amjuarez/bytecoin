// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "ITransfersSynchronizer.h"
#include "TransfersContainer.h"
#include "IObservableImpl.h"

namespace CryptoNote {

class TransfersSubscription : public IObservableImpl < ITransfersObserver, ITransfersSubscription > {
public:

  TransfersSubscription(const cryptonote::Currency& currency, const AccountSubscription& sub);

  SynchronizationStart getSyncStart();
  void onBlockchainDetach(uint64_t height);
  void onError(const std::error_code& ec, uint64_t height);
  bool advanceHeight(uint64_t height);
  const AccountKeys& getKeys() const;
  void addTransaction(const BlockInfo& blockInfo, 
    const ITransactionReader& tx, const std::vector<TransactionOutputInformationIn>& transfers);

  void deleteUnconfirmedTransaction(const Hash& transactionHash);
  void markTransactionConfirmed(const BlockInfo& block, const Hash& transactionHash, const std::vector<uint64_t>& globalIndices);

  // ITransfersSubscription
  virtual AccountAddress getAddress() override;
  virtual ITransfersContainer& getContainer() override;

private:

  TransfersContainer m_transfers;
  AccountSubscription m_subscription;
  const cryptonote::Currency& m_currency;
};

}
