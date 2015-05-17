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

#include "ITransfersSynchronizer.h"
#include "IBlockchainSynchronizer.h"
#include "TypeHelpers.h"

#include <unordered_map>
#include <memory>
#include <cstring>

namespace cryptonote {
class Currency;
}

namespace CryptoNote {
 
class TransfersConsumer;
class INode;

class TransfersSyncronizer : public ITransfersSynchronizer {
public:

  TransfersSyncronizer(const cryptonote::Currency& currency, IBlockchainSynchronizer& sync, INode& node);
  ~TransfersSyncronizer();

  // ITransfersSynchronizer
  virtual ITransfersSubscription& addSubscription(const AccountSubscription& acc) override;
  virtual bool removeSubscription(const AccountAddress& acc) override;
  virtual void getSubscriptions(std::vector<AccountAddress>& subscriptions) override;
  virtual ITransfersSubscription* getSubscription(const AccountAddress& acc) override;

  // IStreamSerializable
  virtual void save(std::ostream& os) override;
  virtual void load(std::istream& in) override;

private:

  // map { view public key -> consumer }
  std::unordered_map<PublicKey, std::unique_ptr<TransfersConsumer>> m_consumers;

  // std::unordered_map<AccountAddress, std::unique_ptr<TransfersConsumer>> m_subscriptions;
  IBlockchainSynchronizer& m_sync;
  INode& m_node;
  const cryptonote::Currency& m_currency;
};

}
