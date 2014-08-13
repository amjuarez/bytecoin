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

#include "WalletUnconfirmedTransactions.h"
#include "cryptonote_core/cryptonote_format_utils.h"

namespace CryptoNote {

bool WalletUnconfirmedTransactions::findTransactionId(const crypto::hash& hash, TransactionId& id) {
  auto it = m_unconfirmedTxs.find(hash);

  if(it == m_unconfirmedTxs.end())
    return false;

  id = it->second.transactionId;

  return true;
}

void WalletUnconfirmedTransactions::erase(const crypto::hash& hash) {
  m_unconfirmedTxs.erase(hash);
}

void WalletUnconfirmedTransactions::add(const cryptonote::transaction& tx,
    TransactionId transactionId, uint64_t change_amount) {
  UnconfirmedTransferDetails& utd = m_unconfirmedTxs[cryptonote::get_transaction_hash(tx)];

  utd.change = change_amount;
  utd.sentTime = time(NULL);
  utd.tx = tx;
  utd.transactionId = transactionId;
}

uint64_t WalletUnconfirmedTransactions::countPendingBalance() const
{
  uint64_t amount = 0;

  for (auto& utx: m_unconfirmedTxs)
    amount+= utx.second.change;

  return amount;
}

} /* namespace CryptoNote */
