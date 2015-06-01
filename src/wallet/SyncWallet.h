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

#pragma once

#include "IWallet.h"
#include <future>

namespace CryptoNote {

// not thread-safe! (sync* methods should be called from one thread)
class SyncWallet: IWalletObserver {

public:

  SyncWallet(IWallet& wallet);
  ~SyncWallet();

  std::error_code syncInitAndLoad(std::istream& source, const std::string& password);
  std::error_code syncSave(std::ostream& destination, bool saveDetailed = true, bool saveCache = true);

private:

  std::error_code callWallet(std::function<void()> f);
  void passResult(std::error_code result);

  virtual void initCompleted(std::error_code result) override;
  virtual void saveCompleted(std::error_code result) override;

  IWallet& m_wallet;
  std::promise<std::error_code>* m_promise;
};


}
