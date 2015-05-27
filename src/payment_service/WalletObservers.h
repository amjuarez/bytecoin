// Copyright (c) 2012-2015, The CryptoNote developers, The Bytecoin developers
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

#include <System/Timer.h>
#include <System/Dispatcher.h>

#include <future>
#include <map>
#include <mutex>

namespace PaymentService {

class WalletLoadObserver : public CryptoNote::IWalletObserver {
public:
  WalletLoadObserver() {}
  virtual ~WalletLoadObserver() {}

  virtual void initCompleted(std::error_code result);

  void waitForLoadEnd();
private:
  std::promise<std::error_code> loadPromise;
};

class WalletSaveObserver : public CryptoNote::IWalletObserver {
public:
  WalletSaveObserver() {}
  virtual ~WalletSaveObserver() {}

  virtual void saveCompleted(std::error_code result);

  void waitForSaveEnd();

private:
  std::promise<std::error_code> savePromise;
};

class WalletTransactionSendObserver : public CryptoNote::IWalletObserver {
public:
  WalletTransactionSendObserver(System::Dispatcher& sys) : system(sys), timer(system) {}
  ~WalletTransactionSendObserver() { timer.stop(); }

  virtual void sendTransactionCompleted(CryptoNote::TransactionId transactionId, std::error_code result);

  void waitForTransactionFinished(CryptoNote::TransactionId transactionId, std::error_code& result);
private:
  std::map<CryptoNote::TransactionId, std::error_code> finishedTransactions;
  std::mutex finishedTransactionsLock;

  System::Dispatcher& system;
  System::Timer timer;
};

} //namespace PaymentService
