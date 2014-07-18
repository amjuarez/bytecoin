// Copyright (c) 2011-2014 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <unordered_map>

#include <time.h>

#include "IWallet.h"
#include "crypto/hash.h"
#include "cryptonote_core/cryptonote_basic.h"

namespace CryptoNote {

struct UnconfirmedTransferDetails
{
  cryptonote::transaction tx;
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
  void add(const cryptonote::transaction& tx, TransactionId transactionId, uint64_t change_amount);

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

