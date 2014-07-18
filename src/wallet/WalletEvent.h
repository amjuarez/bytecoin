// Copyright (c) 2011-2014 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "IWallet.h"
#include "common/ObserverManager.h"

namespace CryptoNote
{

class WalletEvent
{
public:
  virtual ~WalletEvent() {};

  virtual void notify(tools::ObserverManager<CryptoNote::IWalletObserver>& observer) = 0;
};

class WalletTransactionUpdatedEvent : public WalletEvent
{
public:
  WalletTransactionUpdatedEvent(TransactionId transactionId) : m_id(transactionId) {};
  virtual ~WalletTransactionUpdatedEvent() {};

  virtual void notify(tools::ObserverManager<CryptoNote::IWalletObserver>& observer)
  {
    observer.notify(&IWalletObserver::transactionUpdated, m_id);
  }

private:
  TransactionId m_id;
};

class WalletSendTransactionCompletedEvent : public WalletEvent
{
public:
  WalletSendTransactionCompletedEvent(TransactionId transactionId, std::error_code result) : m_id(transactionId), m_error(result) {};
  virtual ~WalletSendTransactionCompletedEvent() {};

  virtual void notify(tools::ObserverManager<CryptoNote::IWalletObserver>& observer)
  {
    observer.notify(&IWalletObserver::sendTransactionCompleted, m_id, m_error);
  }

private:
  TransactionId m_id;
  std::error_code m_error;
};

class WalletExternalTransactionCreatedEvent : public WalletEvent
{
public:
  WalletExternalTransactionCreatedEvent(TransactionId transactionId) : m_id(transactionId) {};
  virtual ~WalletExternalTransactionCreatedEvent() {};

  virtual void notify(tools::ObserverManager<CryptoNote::IWalletObserver>& observer)
  {
    observer.notify(&IWalletObserver::externalTransactionCreated, m_id);
  }
private:
  TransactionId m_id;
};

class WalletSynchronizationProgressUpdatedEvent : public WalletEvent
{
public:
  WalletSynchronizationProgressUpdatedEvent(uint64_t current, uint64_t total, std::error_code result) : m_current(current), m_total(total), m_ec(result) {};
  virtual ~WalletSynchronizationProgressUpdatedEvent() {};

  virtual void notify(tools::ObserverManager<CryptoNote::IWalletObserver>& observer)
  {
    observer.notify(&IWalletObserver::synchronizationProgressUpdated, m_current, m_total, m_ec);
  }

private:
  uint64_t m_current;
  uint64_t m_total;
  std::error_code m_ec;
};


class WalletActualBalanceUpdatedEvent : public WalletEvent
{
public:
  WalletActualBalanceUpdatedEvent(uint64_t balance) : m_balance(balance) {};
  virtual ~WalletActualBalanceUpdatedEvent() {};

  virtual void notify(tools::ObserverManager<CryptoNote::IWalletObserver>& observer)
  {
    observer.notify(&IWalletObserver::actualBalanceUpdated, m_balance);
  }
private:
  uint64_t m_balance;
};

class WalletPendingBalanceUpdatedEvent : public WalletEvent
{
public:
  WalletPendingBalanceUpdatedEvent(uint64_t balance) : m_balance(balance) {};
  virtual ~WalletPendingBalanceUpdatedEvent() {};

  virtual void notify(tools::ObserverManager<CryptoNote::IWalletObserver>& observer)
  {
    observer.notify(&IWalletObserver::pendingBalanceUpdated, m_balance);
  }
private:
  uint64_t m_balance;
};

} /* namespace CryptoNote */

