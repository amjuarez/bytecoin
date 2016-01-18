// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "ITransfersSynchronizer.h"
#include "TransfersContainer.h"
#include "IObservableImpl.h"

namespace CryptoNote {

class TransfersSubscription : public IObservableImpl < ITransfersObserver, ITransfersSubscription > {
public:
  TransfersSubscription(const CryptoNote::Currency& currency, const AccountSubscription& sub);

  SynchronizationStart getSyncStart();
  void onBlockchainDetach(uint32_t height);
  void onError(const std::error_code& ec, uint32_t height);
  bool advanceHeight(uint32_t height);
  const AccountKeys& getKeys() const;
  bool addTransaction(const TransactionBlockInfo& blockInfo, const ITransactionReader& tx,
                      const std::vector<TransactionOutputInformationIn>& transfers);

  void deleteUnconfirmedTransaction(const Crypto::Hash& transactionHash);
  void markTransactionConfirmed(const TransactionBlockInfo& block, const Crypto::Hash& transactionHash, const std::vector<uint32_t>& globalIndices);

  // ITransfersSubscription
  virtual AccountPublicAddress getAddress() override;
  virtual ITransfersContainer& getContainer() override;

private:
  TransfersContainer transfers;
  AccountSubscription subscription;
};

}
