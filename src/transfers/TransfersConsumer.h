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

#include "IBlockchainSynchronizer.h"
#include "ITransfersSynchronizer.h"
#include "TransfersSubscription.h"
#include "TypeHelpers.h"

#include "crypto/crypto.h"

#include "IObservableImpl.h"

#include <unordered_set>

namespace CryptoNote {

class INode;

class TransfersConsumer : public IBlockchainConsumer {

public:

  TransfersConsumer(const cryptonote::Currency& currency, INode& node, const SecretKey& viewSecret);

  ITransfersSubscription& addSubscription(const AccountSubscription& subscription);
  // returns true if no subscribers left
  bool removeSubscription(const AccountAddress& address);
  ITransfersSubscription* getSubscription(const AccountAddress& acc);
  void getSubscriptions(std::vector<AccountAddress>& subscriptions);
  
  // IBlockchainConsumer
  virtual SynchronizationStart getSyncStart() override;
  virtual void onBlockchainDetach(uint64_t height) override;
  virtual bool onNewBlocks(const CompleteBlock* blocks, uint64_t startHeight, size_t count) override;
  virtual std::error_code onPoolUpdated(const std::vector<cryptonote::Transaction>& addedTransactions, const std::vector<crypto::hash>& deletedTransactions) override;
  virtual void getKnownPoolTxIds(std::vector<crypto::hash>& ids) override;

private:

  template <typename F>
  void forEachSubscription(F action) {
    for (const auto& kv : m_subscriptions) {
      action(*kv.second);
    }
  }

  struct PreprocessInfo {
    std::unordered_map<PublicKey, std::vector<uint32_t>> outputs;
    std::vector<uint64_t> globalIdxs;
  };

  std::error_code preprocessOutputs(const BlockInfo& blockInfo, const ITransactionReader& tx, PreprocessInfo& info);
  std::error_code processTransaction(const BlockInfo& blockInfo, const ITransactionReader& tx);
  std::error_code processTransaction(const BlockInfo& blockInfo, const ITransactionReader& tx, const PreprocessInfo& info);
  std::error_code processOutputs(const BlockInfo& blockInfo, TransfersSubscription& sub, const ITransactionReader& tx,
    const std::vector<uint32_t>& outputs, const std::vector<uint64_t>& globalIdxs);

  std::error_code getGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices);

  void updateSyncStart();

  SynchronizationStart m_syncStart;
  const SecretKey m_viewSecret;
  // map { spend public key -> subscription }
  std::unordered_map<PublicKey, std::unique_ptr<TransfersSubscription>> m_subscriptions;
  std::unordered_set<PublicKey> m_spendKeys;

  INode& m_node;
  const cryptonote::Currency& m_currency;
};

}
