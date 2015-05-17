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
