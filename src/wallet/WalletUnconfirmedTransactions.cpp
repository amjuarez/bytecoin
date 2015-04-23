// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "WalletUnconfirmedTransactions.h"
#include "WalletSerialization.h"

#include "cryptonote_core/cryptonote_format_utils.h"
#include "serialization/ISerializer.h"
#include "serialization/SerializationOverloads.h"

namespace CryptoNote {

inline TransactionOutputId getOutputId(const TransactionOutputInformation& out) {
  return std::make_pair(out.transactionPublicKey, out.outputInTransaction);
}

void WalletUnconfirmedTransactions::serialize(cryptonote::ISerializer& s, const std::string& name) {
  s.beginObject(name);
  s(m_unconfirmedTxs, "transactions");
  s.endObject();

  if (s.type() == cryptonote::ISerializer::INPUT) {
    collectUsedOutputs();
  }
}

bool WalletUnconfirmedTransactions::findTransactionId(const TransactionHash& hash, TransactionId& id) {
  auto it = m_unconfirmedTxs.find(hash);
  if (it == m_unconfirmedTxs.end()) {
    return false;
  }

  id = it->second.transactionId;
  return true;
}

void WalletUnconfirmedTransactions::erase(const TransactionHash& hash) {
  auto it = m_unconfirmedTxs.find(hash);
  if (it == m_unconfirmedTxs.end()) {
    return;
  }

  for (const auto& o : it->second.usedOutputs) {
    m_usedOutputs.erase(o);
  }
  m_unconfirmedTxs.erase(it);
}

void WalletUnconfirmedTransactions::add(const cryptonote::Transaction& tx, TransactionId transactionId, 
  uint64_t amount, const std::list<TransactionOutputInformation>& usedOutputs) {

  auto cryptoHash = cryptonote::get_transaction_hash(tx);
  TransactionHash hash = reinterpret_cast<const TransactionHash&>(cryptoHash);

  UnconfirmedTransferDetails& utd = m_unconfirmedTxs[hash];

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

void WalletUnconfirmedTransactions::updateTransactionId(const TransactionHash& hash, TransactionId id) {
  auto it = m_unconfirmedTxs.find(hash);
  if (it != m_unconfirmedTxs.end()) {
    it->second.transactionId = id;
  }
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


} /* namespace CryptoNote */
