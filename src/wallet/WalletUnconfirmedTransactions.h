// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2014-2015 XDN developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "IWallet.h"
#include "ITransfersContainer.h"

#include <unordered_map>
#include <unordered_set>
#include <set>
#include <time.h>
#include <boost/functional/hash.hpp>

#include "crypto/hash.h"
#include "cryptonote_core/cryptonote_basic.h"

#include <transfers/TypeHelpers.h>

namespace cryptonote {
class ISerializer;
}

namespace CryptoNote {

struct UnconfirmedTransferDetails {

  UnconfirmedTransferDetails() :
    amount(0), sentTime(0), transactionId(INVALID_TRANSACTION_ID) {}

  cryptonote::Transaction tx;
  uint64_t amount;
  uint64_t outsAmount;
  time_t sentTime;
  TransactionId transactionId;
  std::vector<std::pair<PublicKey, size_t>> usedOutputs;
};

struct UnconfirmedSpentDepositDetails {
  TransactionId transactionId;
  uint64_t depositsSum;
  uint64_t fee;
};

class WalletUnconfirmedTransactions {
public:

  void serialize(cryptonote::ISerializer& s, const std::string& name);
  void deserializeV1(cryptonote::ISerializer& s, const std::string& name);

  bool findTransactionId(const TransactionHash& hash, TransactionId& id);
  void erase(const TransactionHash& hash);
  void add(const cryptonote::Transaction& tx, TransactionId transactionId, 
    uint64_t amount, const std::vector<TransactionOutputInformation>& usedOutputs);
  void updateTransactionId(const TransactionHash& hash, TransactionId id);

  void addCreatedDeposit(DepositId id, uint64_t totalAmount);
  void addDepositSpendingTransaction(const Hash& transactionHash, const UnconfirmedSpentDepositDetails& details);

  void eraseCreatedDeposit(DepositId id);

  uint64_t countCreatedDepositsSum() const;
  uint64_t countSpentDepositsProfit() const;
  uint64_t countSpentDepositsTotalAmount() const;

  uint64_t countUnconfirmedOutsAmount() const;
  uint64_t countUnconfirmedTransactionsAmount() const;
  bool isUsed(const TransactionOutputInformation& out) const;

private:

  void collectUsedOutputs();

  bool eraseUnconfirmedTransaction(const TransactionHash& hash);
  bool eraseDepositSpendingTransaction(const TransactionHash& hash);

  bool findUnconfirmedTransactionId(const TransactionHash& hash, TransactionId& id);
  bool findUnconfirmedDepositSpendingTransactionId(const TransactionHash& hash, TransactionId& id);

  typedef std::unordered_map<TransactionHash, UnconfirmedTransferDetails, boost::hash<TransactionHash>> UnconfirmedTxsContainer;
  typedef std::set<std::pair<PublicKey, size_t>> UsedOutputsContainer;

  UnconfirmedTxsContainer m_unconfirmedTxs;
  UsedOutputsContainer m_usedOutputs;

  std::unordered_map<DepositId, uint64_t> m_createdDeposits;
  std::unordered_map<Hash, UnconfirmedSpentDepositDetails> m_spentDeposits;
};

} // namespace CryptoNote
