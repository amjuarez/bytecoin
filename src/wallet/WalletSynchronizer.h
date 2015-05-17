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

#include <memory>
#include <vector>
#include <deque>

#include "INode.h"
#include "crypto/hash.h"
#include "WalletTransferDetails.h"
#include "WalletUnconfirmedTransactions.h"
#include "WalletUserTransactionsCache.h"
#include "WalletEvent.h"
#include "WalletSynchronizationContext.h"
#include "WalletRequest.h"

namespace CryptoNote {

class WalletSynchronizer
{
public:
  WalletSynchronizer(const cryptonote::account_base& account, INode& node, std::vector<crypto::hash>& blockchain, WalletTransferDetails& transferDetails,
      WalletUnconfirmedTransactions& unconfirmedTransactions, WalletUserTransactionsCache& transactionsCache);

  std::shared_ptr<WalletRequest> makeStartRefreshRequest();

  void stop();

private:

  struct ProcessParameters {
    std::shared_ptr<SynchronizationContext> context;
    std::vector<std::shared_ptr<WalletEvent> > events;
    boost::optional<std::shared_ptr<WalletRequest> > nextRequest;
  };

  enum NextBlockAction {
    INTERRUPT = 0,
    CONTINUE,
    SKIP
  };

  void handleNewBlocksPortion(std::shared_ptr<SynchronizationContext> context, std::deque<std::shared_ptr<WalletEvent> >& events,
      boost::optional<std::shared_ptr<WalletRequest> >& nextRequest, std::error_code ec);
  void handleTransactionOutGlobalIndicesResponse(std::shared_ptr<SynchronizationContext> context, crypto::hash txid, uint64_t height,
      std::deque<std::shared_ptr<WalletEvent> >& events, boost::optional<std::shared_ptr<WalletRequest> >& nextRequest, std::error_code ec);

  void getShortChainHistory(std::list<crypto::hash>& ids);

  bool processNewBlocks(ProcessParameters& parameters);
  NextBlockAction handleNewBlockchainEntry(ProcessParameters& parameters, cryptonote::block_complete_entry& blockEntry, uint64_t height);
  bool processNewBlockchainEntry(ProcessParameters& parameters, cryptonote::block_complete_entry& blockEntry, const cryptonote::Block& b, crypto::hash& blockId, uint64_t height);
  bool processNewTransaction(ProcessParameters& parameters, const cryptonote::Transaction& tx, uint64_t height, bool isCoinbase, uint64_t timestamp);
  bool processMinersTx(ProcessParameters& parameters, const cryptonote::Transaction& tx, uint64_t height, uint64_t timestamp);
  void processUnconfirmed(ProcessParameters& parameters, const cryptonote::Transaction& tx, uint64_t height, uint64_t timestamp);
  uint64_t processMyInputs(const cryptonote::Transaction& tx);
  void updateTransactionsCache(ProcessParameters& parameters, const cryptonote::Transaction& tx, uint64_t myOuts, uint64_t myInputs, uint64_t height,
      bool isCoinbase, uint64_t timestamp);
  void detachBlockchain(uint64_t height);
  void refreshBalance(std::deque<std::shared_ptr<WalletEvent> >& events);

  void fillGetTransactionOutsGlobalIndicesRequest(ProcessParameters& parameters, const cryptonote::Transaction& tx,
      const std::vector<size_t>& outs, const crypto::public_key& publicKey, uint64_t height);
  void postGetTransactionOutsGlobalIndicesRequest(ProcessParameters& parameters, const crypto::hash& hash, std::vector<uint64_t>& outsGlobalIndices, uint64_t height);
  std::shared_ptr<WalletRequest> makeGetNewBlocksRequest(std::shared_ptr<SynchronizationContext> context);

  const cryptonote::account_base& m_account;
  INode& m_node;
  std::vector<crypto::hash>& m_blockchain;
  WalletTransferDetails& m_transferDetails;
  WalletUnconfirmedTransactions& m_unconfirmedTransactions;
  WalletUserTransactionsCache& m_transactionsCache;

  uint64_t m_actualBalance;
  uint64_t m_pendingBalance;

  bool m_isStoping;
};

} /* namespace CryptoNote */
