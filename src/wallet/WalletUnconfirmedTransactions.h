// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "IWallet.h"
#include "ITransfersContainer.h"

#include <unordered_map>
#include <set>
#include <time.h>
#include <boost/functional/hash.hpp>

#include "crypto/hash.h"
#include "cryptonote_core/cryptonote_basic.h"

namespace cryptonote {
class ISerializer;
}

namespace CryptoNote {

typedef std::pair<PublicKey, size_t> TransactionOutputId;

struct UnconfirmedTransferDetails {

  UnconfirmedTransferDetails() :
    amount(0), sentTime(0), transactionId(INVALID_TRANSACTION_ID) {}

  cryptonote::Transaction tx;
  uint64_t amount;
  uint64_t outsAmount;
  time_t sentTime;
  TransactionId transactionId;
  std::vector<TransactionOutputId> usedOutputs;
};

class WalletUnconfirmedTransactions
{
public:

  void serialize(cryptonote::ISerializer& s, const std::string& name);

  bool findTransactionId(const TransactionHash& hash, TransactionId& id);
  void erase(const TransactionHash& hash);
  void add(const cryptonote::Transaction& tx, TransactionId transactionId, 
    uint64_t amount, const std::list<TransactionOutputInformation>& usedOutputs);
  void updateTransactionId(const TransactionHash& hash, TransactionId id);

  uint64_t countUnconfirmedOutsAmount() const;
  uint64_t countUnconfirmedTransactionsAmount() const;
  bool isUsed(const TransactionOutputInformation& out) const;

private:

  void collectUsedOutputs();

  typedef std::unordered_map<TransactionHash, UnconfirmedTransferDetails, boost::hash<TransactionHash>> UnconfirmedTxsContainer;
  typedef std::set<TransactionOutputId> UsedOutputsContainer;

  UnconfirmedTxsContainer m_unconfirmedTxs;
  UsedOutputsContainer m_usedOutputs;
};

} // namespace CryptoNote
