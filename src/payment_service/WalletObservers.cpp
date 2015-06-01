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

#include "WalletObservers.h"

#include <utility>

namespace PaymentService {

void WalletLoadObserver::initCompleted(std::error_code result) {
  loadPromise.set_value(result);
}

void WalletLoadObserver::waitForLoadEnd() {
  auto future = loadPromise.get_future();

  std::error_code ec = future.get();
  if (ec) {
    throw std::system_error(ec);
  }
  return;
}

void WalletSaveObserver::saveCompleted(std::error_code result) {
  savePromise.set_value(result);
}

void WalletSaveObserver::waitForSaveEnd() {
  auto future = savePromise.get_future();

  std::error_code ec = future.get();
  if (ec) {
    throw std::system_error(ec);
  }
  return;
}

void WalletTransactionSendObserver::sendTransactionCompleted(CryptoNote::TransactionId transactionId, std::error_code result) {
  std::lock_guard<std::mutex> lock(finishedTransactionsLock);
  finishedTransactions.insert(std::make_pair(transactionId, result));
}

void WalletTransactionSendObserver::waitForTransactionFinished(CryptoNote::TransactionId transactionId, std::error_code& result) {
  while (true) {
    {
      std::lock_guard<std::mutex> lock(finishedTransactionsLock);

      auto it = finishedTransactions.find(transactionId);
      if (it != finishedTransactions.end()) {
        result = it->second;
        break;
      }
    }

    timer.sleep(std::chrono::milliseconds(10));
  }
}

} //namespace PaymentService
