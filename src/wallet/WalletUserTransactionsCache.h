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

#include "crypto/hash.h"
#include "IWallet.h"
#include "WalletTxSendingState.h"

namespace CryptoNote {

class WalletUserTransactionsCache
{
public:
  WalletUserTransactionsCache(WalletTxSendingState& states) : m_sendingTxsStates(states) {}

  template <typename Archive>
  void save(Archive& ar, bool saveDetailed, bool saveCache);

  template<typename Archive>
  void load(Archive& ar);

  size_t getTransactionCount() const;
  size_t getTransferCount() const;

  TransactionId findTransactionByTransferId(TransferId transferId) const;

  bool getTransaction(TransactionId transactionId, Transaction& transaction) const;
  Transaction& getTransaction(TransactionId transactionId);
  bool getTransfer(TransferId transferId, Transfer& transfer) const;
  Transfer& getTransfer(TransferId transferId);

  TransactionId insertTransaction(Transaction&& transaction);
  TransferId insertTransfers(const std::vector<Transfer>& transfers);

  TransactionId findTransactionByHash(const crypto::hash& hash);
  void detachTransactions(uint64_t height);

private:
  typedef std::vector<Transfer> UserTransfers;
  typedef std::vector<Transaction> UserTransactions;

  void getGoodItems(bool saveDetailed, UserTransactions& transactions, UserTransfers& transfers);
  void getGoodTransaction(TransactionId txId, size_t offset, bool saveDetailed, UserTransactions& transactions, UserTransfers& transfers);

  void getGoodTransfers(UserTransfers& transfers);
  void getTransfersByTx(TransactionId id, UserTransfers& transfers);

  UserTransactions m_transactions;
  UserTransfers m_transfers;

  WalletTxSendingState& m_sendingTxsStates;
};

template<typename Archive>
void WalletUserTransactionsCache::load(Archive& ar) {
  ar >> m_transactions;
  ar >> m_transfers;
}

template <typename Archive>
void WalletUserTransactionsCache::save(Archive& ar, bool saveDetailed, bool saveCache) {
  UserTransactions txsToSave;
  UserTransfers transfersToSave;

  if (saveCache) {
    getGoodItems(saveDetailed, txsToSave, transfersToSave);
  } else  {
    if (saveDetailed) getGoodTransfers(transfersToSave);
  }

  ar << txsToSave;
  ar << transfersToSave;
}

} //namespace CryptoNote
