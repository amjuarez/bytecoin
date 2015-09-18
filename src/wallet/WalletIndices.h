// Copyright (c) 2012-2015, The CryptoNote developers, The Bytecoin developers
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

#include <unordered_map>

#include "ITransfersContainer.h"
#include "IWallet.h"
#include "IWalletLegacy.h" //TODO: make common types for all of our APIs (such as PublicKey, KeyPair, etc)

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/member.hpp>

namespace CryptoNote {

const uint64_t ACCOUNT_CREATE_TIME_ACCURACY = 60 * 60 * 24;

struct WalletRecord {
  Crypto::PublicKey spendPublicKey;
  Crypto::SecretKey spendSecretKey;
  CryptoNote::ITransfersContainer* container = nullptr;
  uint64_t pendingBalance = 0;
  uint64_t actualBalance = 0;
  time_t creationTimestamp;
};

struct SpentOutput {
  uint64_t amount;
  Crypto::Hash transactionHash;
  uint32_t outputInTransaction;
  const WalletRecord* wallet;
  Crypto::Hash spendingTransactionHash;
};

struct RandomAccessIndex {};
struct KeysIndex {};
struct TransfersContainerIndex {};

struct WalletIndex {};
struct TransactionOutputIndex {};
struct BlockHeightIndex {};

struct TransactionHashIndex {};
struct TransactionIndex {};

typedef boost::multi_index_container <
  WalletRecord,
  boost::multi_index::indexed_by <
    boost::multi_index::random_access < boost::multi_index::tag <RandomAccessIndex> >,
    boost::multi_index::hashed_unique < boost::multi_index::tag <KeysIndex>,
    BOOST_MULTI_INDEX_MEMBER(WalletRecord, Crypto::PublicKey, spendPublicKey)>,
    boost::multi_index::hashed_unique < boost::multi_index::tag <TransfersContainerIndex>,
      BOOST_MULTI_INDEX_MEMBER(WalletRecord, CryptoNote::ITransfersContainer*, container) >
  >
> WalletsContainer;

struct OutputIndex: boost::multi_index::composite_key <
  SpentOutput,
  BOOST_MULTI_INDEX_MEMBER(SpentOutput, Crypto::Hash, transactionHash),
  BOOST_MULTI_INDEX_MEMBER(SpentOutput, uint32_t, outputInTransaction)
> {};

typedef boost::multi_index_container <
  SpentOutput,
  boost::multi_index::indexed_by <
    boost::multi_index::hashed_unique < boost::multi_index::tag <TransactionOutputIndex>,
      OutputIndex
    >,
    boost::multi_index::hashed_non_unique < boost::multi_index::tag <TransactionIndex>,
      BOOST_MULTI_INDEX_MEMBER(SpentOutput, Crypto::Hash, spendingTransactionHash)
    >,
    boost::multi_index::hashed_non_unique < boost::multi_index::tag <WalletIndex>,
      BOOST_MULTI_INDEX_MEMBER(SpentOutput, const WalletRecord *, wallet)
    >
  >
> SpentOutputs;

struct UnlockTransactionJob {
  uint32_t blockHeight;
  CryptoNote::ITransfersContainer* container;
  Crypto::Hash transactionHash;
};

typedef boost::multi_index_container <
  UnlockTransactionJob,
  boost::multi_index::indexed_by <
    boost::multi_index::ordered_non_unique < boost::multi_index::tag <BlockHeightIndex>,
    BOOST_MULTI_INDEX_MEMBER(UnlockTransactionJob, uint32_t, blockHeight)
    >,
    boost::multi_index::hashed_unique < boost::multi_index::tag <TransactionHashIndex>,
      BOOST_MULTI_INDEX_MEMBER(UnlockTransactionJob, Crypto::Hash, transactionHash)
    >
  >
> UnlockTransactionJobs;

typedef boost::multi_index_container <
  CryptoNote::WalletTransaction,
  boost::multi_index::indexed_by <
    boost::multi_index::random_access < boost::multi_index::tag <RandomAccessIndex> >,
    boost::multi_index::hashed_unique < boost::multi_index::tag <TransactionIndex>,
      boost::multi_index::member<CryptoNote::WalletTransaction, Crypto::Hash, &CryptoNote::WalletTransaction::hash >
    >
  >
> WalletTransactions;

typedef std::unordered_map<Crypto::Hash, uint64_t> TransactionChanges;

typedef std::pair<size_t, CryptoNote::WalletTransfer> TransactionTransferPair;
typedef std::vector<TransactionTransferPair> WalletTransfers;

}
