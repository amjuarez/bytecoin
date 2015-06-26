// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "crypto/hash.h"
#include "IWallet.h"
#include "ITransfersContainer.h"
#include <transfers/TypeHelpers.h>

#include "WalletEvent.h"
#include "WalletUnconfirmedTransactions.h"
#include "WalletDepositInfo.h"

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
  size_t getDepositCount() const;

  TransactionId addNewTransaction(uint64_t amount, uint64_t fee, const std::string& extra, const std::vector<Transfer>& transfers, uint64_t unlockTime, const std::vector<TransactionMessage>& messages);
  void updateTransaction(TransactionId transactionId, const cryptonote::Transaction& tx, uint64_t amount, const std::list<TransactionOutputInformation>& usedOutputs);
  void updateTransactionSendingState(TransactionId transactionId, std::error_code ec);

  std::shared_ptr<WalletEvent> onTransactionUpdated(const TransactionInformation& txInfo, int64_t txBalance);
  std::shared_ptr<WalletEvent> onTransactionDeleted(const TransactionHash& transactionHash);

  TransactionId findTransactionByTransferId(TransferId transferId) const;

  bool getTransaction(TransactionId transactionId, TransactionInfo& transaction) const;
  TransactionInfo& getTransaction(TransactionId transactionId);
  bool getTransfer(TransferId transferId, Transfer& transfer) const;
  Transfer& getTransfer(TransferId transferId);
  bool getDeposit(DepositId depositId, Deposit& deposit) const;

  bool isUsed(const TransactionOutputInformation& out) const;

  std::vector<Payments> getTransactionsByPaymentIds(const std::vector<PaymentId>& paymentIds) const;

private:
  TransactionId findTransactionByHash(const TransactionHash& hash);
  TransactionId insertTransaction(TransactionInfo&& Transaction);
  TransferId insertTransfers(const std::vector<Transfer>& transfers);
  void updateUnconfirmedTransactions();

  typedef std::vector<Transfer> UserTransfers;
  typedef std::vector<TransactionInfo> UserTransactions;
  typedef std::vector<DepositInfo> UserDeposits;
  using Offset = UserTransactions::size_type;
  using UserPaymentIndex = std::unordered_map<PaymentId, std::vector<Offset>>;
  
  void rebuildPaymentsIndex();
  void pushToPaymentsIndex(const PaymentId& paymentId, Offset distance);
  void pushToPaymentsIndexInternal(Offset distance, const TransactionInfo& info, std::vector<uint8_t>& extra);
  void popFromPaymentsIndex(const PaymentId& paymentId, Offset distance);

  UserTransactions m_transactions;
  UserTransfers m_transfers;
  UserDeposits m_deposits;
  WalletUnconfirmedTransactions m_unconfirmedTransactions;
  UserPaymentIndex m_paymentsIndex;
};

} //namespace CryptoNote
