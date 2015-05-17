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
  WalletSynchronizationProgressUpdatedEvent(uint64_t current, uint64_t total) : m_current(current), m_total(total) {};
  virtual ~WalletSynchronizationProgressUpdatedEvent() {};

  virtual void notify(tools::ObserverManager<CryptoNote::IWalletObserver>& observer)
  {
    observer.notify(&IWalletObserver::synchronizationProgressUpdated, m_current, m_total);
  }

private:
  uint64_t m_current;
  uint64_t m_total;
};

class WalletSynchronizationCompletedEvent : public WalletEvent {
public:
  WalletSynchronizationCompletedEvent(uint64_t current, uint64_t total, std::error_code result) : m_ec(result) {};
  virtual ~WalletSynchronizationCompletedEvent() {};

  virtual void notify(tools::ObserverManager<CryptoNote::IWalletObserver>& observer) {
    observer.notify(&IWalletObserver::synchronizationCompleted, m_ec);
  }

private:
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
