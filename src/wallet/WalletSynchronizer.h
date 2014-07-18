// Copyright (c) 2011-2014 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
  bool processNewBlockchainEntry(ProcessParameters& parameters, cryptonote::block_complete_entry& blockEntry, const cryptonote::block& b, crypto::hash& blockId, uint64_t height);
  bool processNewTransaction(ProcessParameters& parameters, const cryptonote::transaction& tx, uint64_t height, bool isCoinbase, uint64_t timestamp);
  bool processMinersTx(ProcessParameters& parameters, const cryptonote::transaction& tx, uint64_t height, uint64_t timestamp);
  void processUnconfirmed(ProcessParameters& parameters, const cryptonote::transaction& tx, uint64_t height, uint64_t timestamp);
  uint64_t processMyInputs(const cryptonote::transaction& tx);
  void updateTransactionsCache(ProcessParameters& parameters, const cryptonote::transaction& tx, uint64_t myOuts, uint64_t myInputs, uint64_t height,
      bool isCoinbase, uint64_t timestamp);
  void detachBlockchain(uint64_t height);
  void refreshBalance(std::deque<std::shared_ptr<WalletEvent> >& events);

  void fillGetTransactionOutsGlobalIndicesRequest(ProcessParameters& parameters, const cryptonote::transaction& tx,
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
