// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// epee
#include "misc_log_ex.h"

#include "WalletErrors.h"
#include "WalletUserTransactionsCache.h"
#include "WalletSerialization.h"
#include "WalletUtils.h"

#include "cryptonote_core/cryptonote_format_utils.h"
#include "serialization/ISerializer.h"
#include "serialization/SerializationOverloads.h"
#include <algorithm>

namespace CryptoNote {

void WalletUserTransactionsCache::serialize(cryptonote::ISerializer& s, const std::string& name) {
  s.beginObject(name);

  s(m_transactions, "transactions");
  s(m_transfers, "transfers");
  s(m_unconfirmedTransactions, "unconfirmed");
  s(m_deposits, "deposits");

  if (s.type() == cryptonote::ISerializer::INPUT) {
    updateUnconfirmedTransactions();
  }

  s.endObject();
}

uint64_t WalletUserTransactionsCache::unconfirmedTransactionsAmount() const {
  return m_unconfirmedTransactions.countUnconfirmedTransactionsAmount();
}

uint64_t WalletUserTransactionsCache::unconfrimedOutsAmount() const {
  return m_unconfirmedTransactions.countUnconfirmedOutsAmount();
}

size_t WalletUserTransactionsCache::getTransactionCount() const {
  return m_transactions.size();
}

size_t WalletUserTransactionsCache::getTransferCount() const {
  return m_transfers.size();
}

size_t WalletUserTransactionsCache::getDepositCount() const {
  return m_deposits.size();
}

TransactionId WalletUserTransactionsCache::addNewTransaction(
  uint64_t amount, uint64_t fee, const std::string& extra, const std::vector<Transfer>& transfers, uint64_t unlockTime, const std::vector<TransactionMessage>& messages) {
  
  TransactionInfo transaction;

  transaction.firstTransferId = insertTransfers(transfers);
  transaction.transferCount = transfers.size();
  transaction.firstDepositId = INVALID_DEPOSIT_ID;
  transaction.depositCount = 0;
  transaction.totalAmount = -static_cast<int64_t>(amount);
  transaction.fee = fee;
  transaction.sentTime = time(nullptr);
  transaction.isCoinbase = false;
  transaction.timestamp = 0;
  transaction.extra = extra;
  transaction.blockHeight = UNCONFIRMED_TRANSACTION_HEIGHT;
  transaction.state = TransactionState::Sending;
  transaction.unlockTime = unlockTime;

  for (const TransactionMessage& message : messages) {
    transaction.messages.push_back(message.message);
  }

  return insertTransaction(std::move(transaction));
}

void WalletUserTransactionsCache::updateTransaction(
  TransactionId transactionId, const cryptonote::Transaction& tx, uint64_t amount, const std::list<TransactionOutputInformation>& usedOutputs) {
  m_unconfirmedTransactions.add(tx, transactionId, amount, usedOutputs);
}

void WalletUserTransactionsCache::updateTransactionSendingState(TransactionId transactionId, std::error_code ec) {
  auto& txInfo = m_transactions.at(transactionId);
  if (ec) {
    txInfo.state = ec.value() == cryptonote::error::TX_CANCELLED ? TransactionState::Cancelled : TransactionState::Failed;
    m_unconfirmedTransactions.erase(txInfo.hash);
  } else {
    txInfo.sentTime = time(nullptr); // update sending time
    txInfo.state = TransactionState::Active;
  }
}

std::shared_ptr<WalletEvent> WalletUserTransactionsCache::onTransactionUpdated(const TransactionInformation& txInfo,
                                                                               int64_t txBalance) {
  std::shared_ptr<WalletEvent> event;

  TransactionId id = CryptoNote::INVALID_TRANSACTION_ID;

  if (!m_unconfirmedTransactions.findTransactionId(txInfo.transactionHash, id)) {
    id = findTransactionByHash(txInfo.transactionHash);
  } else {
    m_unconfirmedTransactions.erase(txInfo.transactionHash);
  }

  bool isCoinbase = txInfo.totalAmountIn == 0;

  if (id == CryptoNote::INVALID_TRANSACTION_ID) {
    TransactionInfo transaction;
    transaction.firstTransferId = INVALID_TRANSFER_ID;
    transaction.transferCount = 0;
    transaction.firstDepositId = INVALID_DEPOSIT_ID;
    transaction.depositCount = 0;
    transaction.totalAmount = txBalance;
    transaction.fee = isCoinbase ? 0 : txInfo.totalAmountIn - txInfo.totalAmountOut;
    transaction.sentTime = 0;
    transaction.hash = txInfo.transactionHash;
    transaction.blockHeight = txInfo.blockHeight;
    transaction.isCoinbase = isCoinbase;
    transaction.timestamp = txInfo.timestamp;
    transaction.extra.assign(txInfo.extra.begin(), txInfo.extra.end());
    transaction.state = TransactionState::Active;
    transaction.unlockTime = txInfo.unlockTime;
    transaction.messages = txInfo.messages;

    id = insertTransaction(std::move(transaction));
    // notification event
    event = std::make_shared<WalletExternalTransactionCreatedEvent>(id);
  } else {
    TransactionInfo& tr = getTransaction(id);
    tr.blockHeight = txInfo.blockHeight;
    tr.timestamp = txInfo.timestamp;
    tr.state = TransactionState::Active;
    // notification event
    event = std::make_shared<WalletTransactionUpdatedEvent>(id);
  }

  return event;
}

std::shared_ptr<WalletEvent> WalletUserTransactionsCache::onTransactionDeleted(const TransactionHash& transactionHash) {
  TransactionId id = CryptoNote::INVALID_TRANSACTION_ID;
  if (m_unconfirmedTransactions.findTransactionId(transactionHash, id)) {
    m_unconfirmedTransactions.erase(transactionHash);
    LOG_ERROR("Unconfirmed transaction is deleted: id = " << id << ", hash = " << transactionHash);
    assert(false);
  } else {
    id = findTransactionByHash(transactionHash);
  }

  std::shared_ptr<WalletEvent> event;
  if (id != CryptoNote::INVALID_TRANSACTION_ID) {
    TransactionInfo& tr = getTransaction(id);
    tr.blockHeight = UNCONFIRMED_TRANSACTION_HEIGHT;
    tr.timestamp = 0;
    tr.state = TransactionState::Deleted;

    event = std::make_shared<WalletTransactionUpdatedEvent>(id);
  } else {
    LOG_ERROR("Transaction wasn't found: " << transactionHash);
    assert(false);
  }

  return event;
}

TransactionId WalletUserTransactionsCache::findTransactionByTransferId(TransferId transferId) const
{
  TransactionId id;
  for (id = 0; id < m_transactions.size(); ++id) {
    const TransactionInfo& tx = m_transactions[id];

    if (tx.firstTransferId == INVALID_TRANSFER_ID || tx.transferCount == 0)
      continue;

    if (transferId >= tx.firstTransferId && transferId < (tx.firstTransferId + tx.transferCount))
      break;
  }

  if (id == m_transactions.size())
    return INVALID_TRANSACTION_ID;

  return id;
}

bool WalletUserTransactionsCache::getTransaction(TransactionId transactionId, TransactionInfo& transaction) const
{
  if (transactionId >= m_transactions.size())
    return false;

  transaction = m_transactions[transactionId];

  return true;
}

bool WalletUserTransactionsCache::getTransfer(TransferId transferId, Transfer& transfer) const
{
  if (transferId >= m_transfers.size())
    return false;

  transfer = m_transfers[transferId];

  return true;
}

bool WalletUserTransactionsCache::getDeposit(DepositId depositId, Deposit& deposit) const {
  if (depositId >= m_deposits.size()) {
    return false;
  }

  deposit = m_deposits[depositId].deposit;
  return true;
}

TransactionId WalletUserTransactionsCache::insertTransaction(TransactionInfo&& Transaction) {
  m_transactions.emplace_back(std::move(Transaction));
  return m_transactions.size() - 1;
}

TransactionId WalletUserTransactionsCache::findTransactionByHash(const TransactionHash& hash) {
  auto it = std::find_if(m_transactions.begin(), m_transactions.end(), [&hash] (const TransactionInfo& tx) { return tx.hash ==  hash; });

  if (it == m_transactions.end())
    return CryptoNote::INVALID_TRANSACTION_ID;

  return std::distance(m_transactions.begin(), it);
}

bool WalletUserTransactionsCache::isUsed(const TransactionOutputInformation& out) const {
  return m_unconfirmedTransactions.isUsed(out);
}

TransactionInfo& WalletUserTransactionsCache::getTransaction(TransactionId transactionId) {
  return m_transactions.at(transactionId);
}

TransferId WalletUserTransactionsCache::insertTransfers(const std::vector<Transfer>& transfers) {
  std::copy(transfers.begin(), transfers.end(), std::back_inserter(m_transfers));
  return m_transfers.size() - transfers.size();
}

void WalletUserTransactionsCache::updateUnconfirmedTransactions() {
  for (TransactionId id = 0; id < m_transactions.size(); ++id) {
    if (m_transactions[id].blockHeight == UNCONFIRMED_TRANSACTION_HEIGHT) {
      m_unconfirmedTransactions.updateTransactionId(m_transactions[id].hash, id);
    }
  }
}

Transfer& WalletUserTransactionsCache::getTransfer(TransferId transferId) {
  return m_transfers.at(transferId);
}

} //namespace CryptoNote
