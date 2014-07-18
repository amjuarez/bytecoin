// Copyright (c) 2011-2014 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
