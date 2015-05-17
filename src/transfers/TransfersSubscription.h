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
