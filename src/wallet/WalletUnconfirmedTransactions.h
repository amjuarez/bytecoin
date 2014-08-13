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

#include <unordered_map>

#include <time.h>

#include "IWallet.h"
#include "crypto/hash.h"
#include "cryptonote_core/cryptonote_basic.h"

namespace CryptoNote {

struct UnconfirmedTransferDetails
{
  cryptonote::Transaction tx;
  uint64_t change;
  time_t sentTime;
  TransactionId transactionId;
};

class WalletUnconfirmedTransactions
{
public:
  template <typename Archive>
  void save(Archive& ar, bool saveCache) const;

  template<typename Archive>
  void load(Archive& ar);

  bool findTransactionId(const crypto::hash& hash, TransactionId& id);
  void erase(const crypto::hash& hash);
  void add(const cryptonote::Transaction& tx, TransactionId transactionId, uint64_t change_amount);

  uint64_t countPendingBalance() const;

private:
  typedef std::unordered_map<crypto::hash, UnconfirmedTransferDetails> UnconfirmedTxsContainer;
  UnconfirmedTxsContainer m_unconfirmedTxs;
};

template <typename Archive>
void WalletUnconfirmedTransactions::save(Archive& ar, bool saveCache) const
{
  const UnconfirmedTxsContainer& unconfirmedTxs = saveCache ? m_unconfirmedTxs : UnconfirmedTxsContainer();
  ar << unconfirmedTxs;
}

template<typename Archive>
void WalletUnconfirmedTransactions::load(Archive& ar)
{
  ar >> m_unconfirmedTxs;
}

} // namespace CryptoNote
