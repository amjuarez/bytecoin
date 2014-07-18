// Copyright (c) 2011-2014 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "WalletUserTransactionsCache.h"

#include <algorithm>

namespace {
bool hashesEqual(const CryptoNote::TransactionHash& h1, const crypto::hash& h2) {
  return !memcmp(static_cast<const void *>(h1.data()), static_cast<const void *>(&h2), h1.size());
}
}

namespace CryptoNote {

size_t WalletUserTransactionsCache::getTransactionCount() const
{
  return m_transactions.size();
}

size_t WalletUserTransactionsCache::getTransferCount() const
{
  return m_transfers.size();
}

TransactionId WalletUserTransactionsCache::findTransactionByTransferId(TransferId transferId) const
{
  TransactionId id;
  for (id = 0; id < m_transactions.size(); ++id) {
    const Transaction& tx = m_transactions[id];

    if (tx.firstTransferId == INVALID_TRANSFER_ID || tx.transferCount == 0)
      continue;

    if (transferId >= tx.firstTransferId && transferId < (tx.firstTransferId + tx.transferCount))
      break;
  }

  if (id == m_transactions.size())
    return INVALID_TRANSACTION_ID;

  return id;
}

bool WalletUserTransactionsCache::getTransaction(TransactionId transactionId, Transaction& transaction) const
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

TransactionId WalletUserTransactionsCache::insertTransaction(Transaction&& transaction) {
  m_transactions.emplace_back(transaction);
  return m_transactions.size() - 1;
}

TransactionId WalletUserTransactionsCache::findTransactionByHash(const crypto::hash& hash) {
  auto it = std::find_if(m_transactions.begin(), m_transactions.end(), [&hash] (const Transaction& tx) { return hashesEqual(tx.hash, hash); });

  if (it == m_transactions.end())
    return CryptoNote::INVALID_TRANSACTION_ID;

  return std::distance(m_transactions.begin(), it);
}

void WalletUserTransactionsCache::detachTransactions(uint64_t height) {
  for (size_t id = 0; id < m_transactions.size(); ++id) {
    Transaction& tx = m_transactions[id];
    if (tx.blockHeight >= height) {
      tx.blockHeight = UNCONFIRMED_TRANSACTION_HEIGHT;
      tx.timestamp = 0;
    }
  }
}

Transaction& WalletUserTransactionsCache::getTransaction(TransactionId transactionId) {
  return m_transactions.at(transactionId);
}

void WalletUserTransactionsCache::getGoodItems(bool saveDetailed, UserTransactions& transactions, UserTransfers& transfers) {
  size_t offset = 0;

  for (size_t txId = 0; txId < m_transactions.size(); ++txId) {
    WalletTxSendingState::State state = m_sendingTxsStates.state(txId);
    bool isGood = state != WalletTxSendingState::ERRORED;

    if (isGood) {
      getGoodTransaction(txId, offset, saveDetailed, transactions, transfers);
    }
    else
    {
      const Transaction& t = m_transactions[txId];
      if (t.firstTransferId != INVALID_TRANSFER_ID)
        offset += t.transferCount;
    }
  }
}

void WalletUserTransactionsCache::getGoodTransaction(TransactionId txId, size_t offset, bool saveDetailed, UserTransactions& transactions, UserTransfers& transfers) {
  transactions.push_back(m_transactions[txId]);
  Transaction& tx = transactions.back();

  if (!saveDetailed) {
    tx.firstTransferId = INVALID_TRANSFER_ID;
    tx.transferCount = 0;

    return;
  }

  if (tx.firstTransferId == INVALID_TRANSFER_ID) {
    return;
  }

  UserTransfers::const_iterator first = m_transfers.begin() + tx.firstTransferId;
  UserTransfers::const_iterator last = first + tx.transferCount;

  tx.firstTransferId -= offset;

  std::copy(first, last, std::back_inserter(transfers));
}

void WalletUserTransactionsCache::getGoodTransfers(UserTransfers& transfers) {
  for (size_t txId = 0; txId < m_transactions.size(); ++txId) {
    WalletTxSendingState::State state = m_sendingTxsStates.state(txId);

    if (state != WalletTxSendingState::ERRORED) {
      getTransfersByTx(txId, transfers);
    }
  }
}

void WalletUserTransactionsCache::getTransfersByTx(TransactionId id, UserTransfers& transfers) {
  const Transaction& tx = m_transactions[id];

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

Transfer& WalletUserTransactionsCache::getTransfer(TransferId transferId) {
  return m_transfers.at(transferId);
}

} //namespace CryptoNote

