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
#include "ITransfersContainer.h"

#include "WalletEvent.h"
#include "WalletUnconfirmedTransactions.h"

namespace cryptonote {
class ISerializer;
}

namespace CryptoNote {

class WalletUserTransactionsCache
{
public:
  WalletUserTransactionsCache() {}

  void serialize(cryptonote::ISerializer& serializer, const std::string& name);

  uint64_t unconfirmedTransactionsAmount() const;
  uint64_t unconfrimedOutsAmount() const;
  size_t getTransactionCount() const;
  size_t getTransferCount() const;

  TransactionId addNewTransaction(uint64_t amount, uint64_t fee, const std::string& extra, const std::vector<Transfer>& transfers, uint64_t unlockTime);
  void updateTransaction(TransactionId transactionId, const cryptonote::Transaction& tx, uint64_t amount, const std::list<TransactionOutputInformation>& usedOutputs);
  void updateTransactionSendingState(TransactionId transactionId, std::error_code ec);

  std::shared_ptr<WalletEvent> onTransactionUpdated(const TransactionInformation& txInfo, int64_t txBalance);
  std::shared_ptr<WalletEvent> onTransactionDeleted(const TransactionHash& transactionHash);

  TransactionId findTransactionByTransferId(TransferId transferId) const;

  bool getTransaction(TransactionId transactionId, TransactionInfo& transaction) const;
  TransactionInfo& getTransaction(TransactionId transactionId);
  bool getTransfer(TransferId transferId, Transfer& transfer) const;
  Transfer& getTransfer(TransferId transferId);

  bool isUsed(const TransactionOutputInformation& out) const;

private:

  TransactionId findTransactionByHash(const TransactionHash& hash);
  TransactionId insertTransaction(TransactionInfo&& Transaction);
  TransferId insertTransfers(const std::vector<Transfer>& transfers);
  void updateUnconfirmedTransactions();

  typedef std::vector<Transfer> UserTransfers;
  typedef std::vector<TransactionInfo> UserTransactions;

  void getGoodItems(UserTransactions& transactions, UserTransfers& transfers);
  void getGoodTransaction(TransactionId txId, size_t offset, UserTransactions& transactions, UserTransfers& transfers);

  void getTransfersByTx(TransactionId id, UserTransfers& transfers);

  UserTransactions m_transactions;
  UserTransfers m_transfers;
  WalletUnconfirmedTransactions m_unconfirmedTransactions;
};

} //namespace CryptoNote
