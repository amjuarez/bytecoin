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

// epee
#include "misc_log_ex.h"

#include "WalletErrors.h"
#include "WalletUserTransactionsCache.h"
#include "WalletSerialization.h"
#include "WalletUtils.h"

#include "serialization/ISerializer.h"
#include "serialization/SerializationOverloads.h"
#include <algorithm>

namespace CryptoNote {


void WalletUserTransactionsCache::serialize(cryptonote::ISerializer& s, const std::string& name) {
  s.beginObject(name);

  if (s.type() == cryptonote::ISerializer::INPUT) {
    s(m_transactions, "transactions");
    s(m_transfers, "transfers");
    s(m_unconfirmedTransactions, "unconfirmed");
    updateUnconfirmedTransactions();
  } else {
    UserTransactions txsToSave;
    UserTransfers transfersToSave;

    getGoodItems(txsToSave, transfersToSave);
    s(txsToSave, "transactions");
    s(transfersToSave, "transfers");
    s(m_unconfirmedTransactions, "unconfirmed");
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

TransactionId WalletUserTransactionsCache::addNewTransaction(
  uint64_t amount, uint64_t fee, const std::string& extra, const std::vector<Transfer>& transfers, uint64_t unlockTime) {
  
  TransactionInfo transaction;

  transaction.firstTransferId = insertTransfers(transfers);
  transaction.transferCount = transfers.size();
  transaction.totalAmount = -static_cast<int64_t>(amount);
  transaction.fee = fee;
  transaction.sentTime = time(nullptr);
  transaction.isCoinbase = false;
  transaction.timestamp = 0;
  transaction.extra = extra;
  transaction.blockHeight = UNCONFIRMED_TRANSACTION_HEIGHT;
  transaction.state = TransactionState::Sending;
  transaction.unlockTime = unlockTime;

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

void WalletUserTransactionsCache::getGoodItems(UserTransactions& transactions, UserTransfers& transfers) {
  size_t offset = 0;

  for (size_t txId = 0; txId < m_transactions.size(); ++txId) {
    bool isGood =
      m_transactions[txId].state != TransactionState::Cancelled &&
      m_transactions[txId].state != TransactionState::Failed;

    if (isGood) {
      getGoodTransaction(txId, offset, transactions, transfers);
    } else {
      const TransactionInfo& t = m_transactions[txId];
      offset += t.firstTransferId != INVALID_TRANSFER_ID ? t.transferCount : 0;
    }
  }
}

void WalletUserTransactionsCache::getGoodTransaction(TransactionId txId, size_t offset, UserTransactions& transactions, UserTransfers& transfers) {
  transactions.push_back(m_transactions[txId]);
  TransactionInfo& tx = transactions.back();

  if (tx.firstTransferId == INVALID_TRANSFER_ID) {
    return;
  }

  UserTransfers::const_iterator first = m_transfers.begin() + tx.firstTransferId;
  UserTransfers::const_iterator last = first + tx.transferCount;

  tx.firstTransferId -= offset;

  std::copy(first, last, std::back_inserter(transfers));
}

void WalletUserTransactionsCache::getTransfersByTx(TransactionId id, UserTransfers& transfers) {
  const TransactionInfo& tx = m_transactions[id];

  if (tx.firstTransferId != INVALID_TRANSFER_ID) {
    UserTransfers::const_iterator first = m_transfers.begin() + tx.firstTransferId;
    UserTransfers::const_iterator last = first + tx.transferCount;
    std::copy(first, last, std::back_inserter(transfers));
  }
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
