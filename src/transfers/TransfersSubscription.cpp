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

#include "TransfersSubscription.h"
#include "IWallet.h"

namespace CryptoNote {

TransfersSubscription::TransfersSubscription(const cryptonote::Currency& currency, const AccountSubscription& sub)
  : m_currency(currency), m_subscription(sub), m_transfers(currency, sub.transactionSpendableAge) {}


SynchronizationStart TransfersSubscription::getSyncStart() {
  return m_subscription.syncStart;
}

void TransfersSubscription::onBlockchainDetach(uint64_t height) {
  std::vector<Hash> deletedTransactions = m_transfers.detach(height);
  for (auto& hash : deletedTransactions) {
    m_observerManager.notify(&ITransfersObserver::onTransactionDeleted, this, hash);
  }
}

void TransfersSubscription::onError(const std::error_code& ec, uint64_t height) {
  if (height != UNCONFIRMED_TRANSACTION_HEIGHT) {
    m_transfers.detach(height);
  }
  m_observerManager.notify(&ITransfersObserver::onError, this, height, ec);
}

bool TransfersSubscription::advanceHeight(uint64_t height) {
  return m_transfers.advanceHeight(height);
}

const AccountKeys& TransfersSubscription::getKeys() const {
  return m_subscription.keys;
}

void TransfersSubscription::addTransaction(const BlockInfo& blockInfo, const ITransactionReader& tx,
                                           const std::vector<TransactionOutputInformationIn>& transfers) {

  bool added = m_transfers.addTransaction(blockInfo, tx, transfers);
  if (added) {
    m_observerManager.notify(&ITransfersObserver::onTransactionUpdated, this, tx.getTransactionHash());
  }
}

AccountAddress TransfersSubscription::getAddress() {
  return m_subscription.keys.address;
}

ITransfersContainer& TransfersSubscription::getContainer() {
  return m_transfers;
}

void TransfersSubscription::deleteUnconfirmedTransaction(const Hash& transactionHash) {
  m_transfers.deleteUnconfirmedTransaction(transactionHash);
  m_observerManager.notify(&ITransfersObserver::onTransactionDeleted, this, transactionHash);
}

void TransfersSubscription::markTransactionConfirmed(const BlockInfo& block, const Hash& transactionHash,
                                                     const std::vector<uint64_t>& globalIndices) {
  m_transfers.markTransactionConfirmed(block, transactionHash, globalIndices);
  m_observerManager.notify(&ITransfersObserver::onTransactionUpdated, this, transactionHash);
}

}
