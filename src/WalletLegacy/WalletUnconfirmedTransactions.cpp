// Copyright (c) 2011-2016 The Cryptonote developers
// Copyright (c) 2014-2017 XDN-project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "WalletUnconfirmedTransactions.h"
#include "WalletLegacy/WalletLegacySerialization.h"

#include "CryptoNoteCore/CryptoNoteTools.h"
#include "Serialization/ISerializer.h"
#include "Serialization/SerializationOverloads.h"

using namespace Crypto;

namespace CryptoNote {

inline TransactionOutputId getOutputId(const TransactionOutputInformation& out) {
  return std::make_pair(out.transactionPublicKey, out.outputInTransaction);
}

WalletUnconfirmedTransactions::WalletUnconfirmedTransactions(uint64_t uncofirmedTransactionsLiveTime):
  m_uncofirmedTransactionsLiveTime(uncofirmedTransactionsLiveTime) {

}

bool WalletUnconfirmedTransactions::serialize(ISerializer& s) {
  s(m_unconfirmedTxs, "transactions");
  s(m_createdDeposits, "unconfirmedCreatedDeposits");
  s(m_spentDeposits, "unconfirmedSpentDeposits");

  if (s.type() == ISerializer::INPUT) {
    collectUsedOutputs();
  }

  return true;
}

bool WalletUnconfirmedTransactions::deserializeV1(ISerializer& s) {
  s(m_unconfirmedTxs, "transactions");

  if (s.type() == ISerializer::INPUT) {
    collectUsedOutputs();
  }

  return true;
}

bool WalletUnconfirmedTransactions::findTransactionId(const Hash& hash, TransactionId& id) {
  return findUnconfirmedTransactionId(hash, id) || findUnconfirmedDepositSpendingTransactionId(hash, id);
}

bool WalletUnconfirmedTransactions::findUnconfirmedTransactionId(const Crypto::Hash& hash, TransactionId& id) {
  auto it = m_unconfirmedTxs.find(hash);
  if (it == m_unconfirmedTxs.end()) {
    return false;
  }

  id = it->second.transactionId;
  return true;
}

bool WalletUnconfirmedTransactions::findUnconfirmedDepositSpendingTransactionId(const Crypto::Hash& hash, TransactionId& id) {
  auto it = m_spentDeposits.find(hash);
  if (it == m_spentDeposits.end()) {
    return false;
  }

  id = it->second.transactionId;
  return true;
}

void WalletUnconfirmedTransactions::erase(const Hash& hash) {
  eraseUnconfirmedTransaction(hash) || eraseDepositSpendingTransaction(hash);
}

bool WalletUnconfirmedTransactions::eraseUnconfirmedTransaction(const Crypto::Hash& hash) {
  auto it = m_unconfirmedTxs.find(hash);
  if (it == m_unconfirmedTxs.end()) {
    return false;
  }

  deleteUsedOutputs(it->second.usedOutputs);
  m_unconfirmedTxs.erase(it);

  return true;
}

bool WalletUnconfirmedTransactions::eraseDepositSpendingTransaction(const Crypto::Hash& hash) {
  auto it = m_spentDeposits.find(hash);
  if (it == m_spentDeposits.end()) {
    return false;
  }

  m_spentDeposits.erase(it);

  return true;
}

void WalletUnconfirmedTransactions::add(const Transaction& tx, TransactionId transactionId, 
  uint64_t amount, const std::vector<TransactionOutputInformation>& usedOutputs) {

  UnconfirmedTransferDetails& utd = m_unconfirmedTxs[getObjectHash(tx)];

  utd.amount = amount;
  utd.sentTime = time(nullptr);
  utd.tx = tx;
  utd.transactionId = transactionId;

  uint64_t outsAmount = 0;
  // process used outputs
  utd.usedOutputs.reserve(usedOutputs.size());
  for (const auto& out : usedOutputs) {
    auto id = getOutputId(out);
    utd.usedOutputs.push_back(id);
    m_usedOutputs.insert(id);
    outsAmount += out.amount;
  }

  utd.outsAmount = outsAmount;
}

void WalletUnconfirmedTransactions::updateTransactionId(const Hash& hash, TransactionId id) {
  auto it = m_unconfirmedTxs.find(hash);
  if (it != m_unconfirmedTxs.end()) {
    it->second.transactionId = id;
  }
}

void WalletUnconfirmedTransactions::addCreatedDeposit(DepositId id, uint64_t totalAmount) {
  m_createdDeposits[id] = totalAmount;
}

void WalletUnconfirmedTransactions::addDepositSpendingTransaction(const Hash& transactionHash, const UnconfirmedSpentDepositDetails& details) {
  assert(m_spentDeposits.count(transactionHash) == 0);
  m_spentDeposits.emplace(transactionHash, details);
}

void WalletUnconfirmedTransactions::eraseCreatedDeposit(DepositId id) {
  m_createdDeposits.erase(id);
}

uint64_t WalletUnconfirmedTransactions::countCreatedDepositsSum() const {
  uint64_t sum = 0;

  for (const auto& kv: m_createdDeposits) {
    sum += kv.second;
  }

  return sum;
}

uint64_t WalletUnconfirmedTransactions::countSpentDepositsProfit() const {
  uint64_t sum = 0;

  for (const auto& kv: m_spentDeposits) {
    sum += kv.second.depositsSum - kv.second.fee;
  }

  return sum;
}

uint64_t WalletUnconfirmedTransactions::countSpentDepositsTotalAmount() const {
  uint64_t sum = 0;

  for (const auto& kv: m_spentDeposits) {
    sum += kv.second.depositsSum;
  }

  return sum;
}

uint64_t WalletUnconfirmedTransactions::countUnconfirmedOutsAmount() const {
  uint64_t amount = 0;

  for (auto& utx: m_unconfirmedTxs)
    amount+= utx.second.outsAmount;

  return amount;
}

uint64_t WalletUnconfirmedTransactions::countUnconfirmedTransactionsAmount() const {
  uint64_t amount = 0;

  for (auto& utx: m_unconfirmedTxs)
    amount+= utx.second.amount;

  return amount;
}

bool WalletUnconfirmedTransactions::isUsed(const TransactionOutputInformation& out) const {
  return m_usedOutputs.find(getOutputId(out)) != m_usedOutputs.end();
}

void WalletUnconfirmedTransactions::collectUsedOutputs() {
  UsedOutputsContainer used;
  for (const auto& kv : m_unconfirmedTxs) {
    used.insert(kv.second.usedOutputs.begin(), kv.second.usedOutputs.end());
  }
  m_usedOutputs = std::move(used);
}

void WalletUnconfirmedTransactions::reset() {
  m_unconfirmedTxs.clear();
  m_usedOutputs.clear();
}

void WalletUnconfirmedTransactions::deleteUsedOutputs(const std::vector<TransactionOutputId>& usedOutputs) {
  for (const auto& output: usedOutputs) {
    m_usedOutputs.erase(output);
  }
}

std::vector<TransactionId> WalletUnconfirmedTransactions::deleteOutdatedTransactions() {
  std::vector<TransactionId> deletedTransactions;

  uint64_t now = static_cast<uint64_t>(time(nullptr));
  assert(now >= m_uncofirmedTransactionsLiveTime);

  for (auto it = m_unconfirmedTxs.begin(); it != m_unconfirmedTxs.end();) {
    if (static_cast<uint64_t>(it->second.sentTime) <= now - m_uncofirmedTransactionsLiveTime) {
      deleteUsedOutputs(it->second.usedOutputs);
      deletedTransactions.push_back(it->second.transactionId);
      it = m_unconfirmedTxs.erase(it);
    } else {
      ++it;
    }
  }

  return deletedTransactions;
}

} /* namespace CryptoNote */
