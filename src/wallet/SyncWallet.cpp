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

#include "SyncWallet.h"
#include <cassert>

#include <cassert>

namespace CryptoNote {


SyncWallet::SyncWallet(IWallet& wallet) : m_wallet(wallet), m_promise(nullptr) {
  m_wallet.addObserver(this);
}

SyncWallet::~SyncWallet() {
  m_wallet.removeObserver(this);
}

std::error_code SyncWallet::callWallet(std::function<void()> f) {
  assert(m_promise == nullptr);

  std::promise<std::error_code> prom;
  m_promise = &prom;

  f();

  auto result = prom.get_future().get();
  m_promise = nullptr;

  return result;
}

void SyncWallet::passResult(std::error_code result) {
  if (m_promise != nullptr) {
    m_promise->set_value(result);
  }
}

std::error_code SyncWallet::syncInitAndLoad(std::istream& source, const std::string& password) {
  return callWallet([&]{ m_wallet.initAndLoad(source, password); });
}

std::error_code SyncWallet::syncSave(std::ostream& destination, bool saveDetailed, bool saveCache) {
  return callWallet([&]{ m_wallet.save(destination, saveDetailed, saveCache); });
}

void SyncWallet::initCompleted(std::error_code result) {
  passResult(result);
}

void SyncWallet::saveCompleted(std::error_code result) {
  passResult(result);
}

}
