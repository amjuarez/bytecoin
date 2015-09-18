// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "ITransfersSynchronizer.h"
#include "IBlockchainSynchronizer.h"
#include "TypeHelpers.h"

#include <unordered_map>
#include <memory>
#include <cstring>

namespace CryptoNote {
class Currency;
}

namespace CryptoNote {
 
class TransfersConsumer;
class INode;

class TransfersSyncronizer : public ITransfersSynchronizer {
public:

  TransfersSyncronizer(const CryptoNote::Currency& currency, IBlockchainSynchronizer& sync, INode& node);
  ~TransfersSyncronizer();

  // ITransfersSynchronizer
  virtual ITransfersSubscription& addSubscription(const AccountSubscription& acc) override;
  virtual bool removeSubscription(const AccountPublicAddress& acc) override;
  virtual void getSubscriptions(std::vector<AccountPublicAddress>& subscriptions) override;
  virtual ITransfersSubscription* getSubscription(const AccountPublicAddress& acc) override;

  // IStreamSerializable
  virtual void save(std::ostream& os) override;
  virtual void load(std::istream& in) override;

private:

  // map { view public key -> consumer }
  std::unordered_map<Crypto::PublicKey, std::unique_ptr<TransfersConsumer>> m_consumers;

  // std::unordered_map<AccountAddress, std::unique_ptr<TransfersConsumer>> m_subscriptions;
  IBlockchainSynchronizer& m_sync;
  INode& m_node;
  const CryptoNote::Currency& m_currency;
};

}
