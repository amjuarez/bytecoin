// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "TransfersSubscription.h"
#include "IWallet.h"

namespace CryptoNote {

TransfersSubscription::TransfersSubscription(const cryptonote::Currency& currency, const AccountSubscription& sub)
  : m_currency(currency), m_subscription(sub), m_transfers(currency, sub.transactionSpendableAge) {}


SynchronizationStart TransfersSubscription::getSyncStart() {
  return m_subscription.syncStart;
}

void TransfersSubscription::onBlockchainDetach(uint64_t height) {
  std::vector<Hash> deletedTransactions;
  std::vector<TransactionOutputInformation> lockedTransfers;
  m_transfers.detach(height, deletedTransactions, lockedTransfers);

  for (auto& hash : deletedTransactions) {
    m_observerManager.notify(&ITransfersObserver::onTransactionDeleted, this, hash);
  }

  if (!lockedTransfers.empty()) {
    m_observerManager.notify(&ITransfersObserver::onTransfersLocked, this, lockedTransfers);
  }
}

void TransfersSubscription::onError(const std::error_code& ec, uint64_t height) {
  if (height != UNCONFIRMED_TRANSACTION_HEIGHT) {
    onBlockchainDetach(height);
  }

  m_observerManager.notify(&ITransfersObserver::onError, this, height, ec);
}

bool TransfersSubscription::advanceHeight(uint64_t height) {
  std::vector<TransactionOutputInformation> unlockedTransfers = m_transfers.advanceHeight(height);

  if (!unlockedTransfers.empty()) {
    m_observerManager.notify(&ITransfersObserver::onTransfersUnlocked, this, unlockedTransfers);
  }

  return true;
}

const AccountKeys& TransfersSubscription::getKeys() const {
  return m_subscription.keys;
}

void TransfersSubscription::addTransaction(const BlockInfo& blockInfo, const ITransactionReader& tx,
                                           const std::vector<TransactionOutputInformationIn>& transfers,
                                           std::vector<std::string>&& messages) {
  std::vector<TransactionOutputInformation> unlockedTransfers;

  bool added = m_transfers.addTransaction(blockInfo, tx, transfers, std::move(messages), &unlockedTransfers);
  if (added) {
    m_observerManager.notify(&ITransfersObserver::onTransactionUpdated, this, tx.getTransactionHash());
  }

  if (!unlockedTransfers.empty()) {
    m_observerManager.notify(&ITransfersObserver::onTransfersUnlocked, this, unlockedTransfers);
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
