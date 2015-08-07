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

#include "TransfersSynchronizer.h"
#include "TransfersConsumer.h"

#include "Common/StdInputStream.h"
#include "Common/StdOutputStream.h"
#include "Serialization/BinaryInputStreamSerializer.h"
#include "Serialization/BinaryOutputStreamSerializer.h"

using namespace Common;
using namespace Crypto;

namespace CryptoNote {

const uint32_t TRANSFERS_STORAGE_ARCHIVE_VERSION = 0;

TransfersSyncronizer::TransfersSyncronizer(const CryptoNote::Currency& currency, IBlockchainSynchronizer& sync, INode& node) :
  m_currency(currency), m_sync(sync), m_node(node) {
}

TransfersSyncronizer::~TransfersSyncronizer() {
  m_sync.stop();
  for (const auto& kv : m_consumers) {
    m_sync.removeConsumer(kv.second.get());
  }
}

ITransfersSubscription& TransfersSyncronizer::addSubscription(const AccountSubscription& acc) {
  auto it = m_consumers.find(acc.keys.address.viewPublicKey);

  if (it == m_consumers.end()) {
    std::unique_ptr<TransfersConsumer> consumer(
      new TransfersConsumer(m_currency, m_node, acc.keys.viewSecretKey));
    m_sync.addConsumer(consumer.get());
    it = m_consumers.insert(std::make_pair(acc.keys.address.viewPublicKey, std::move(consumer))).first;
  }
    
  return it->second->addSubscription(acc);
}

bool TransfersSyncronizer::removeSubscription(const AccountPublicAddress& acc) {
  auto it = m_consumers.find(acc.viewPublicKey);
  if (it == m_consumers.end())
    return false;

  if (it->second->removeSubscription(acc)) {
    m_sync.removeConsumer(it->second.get());
    m_consumers.erase(it);
  }

  return true;
}

void TransfersSyncronizer::getSubscriptions(std::vector<AccountPublicAddress>& subscriptions) {
  for (const auto& kv : m_consumers) {
    kv.second->getSubscriptions(subscriptions);
  }
}

ITransfersSubscription* TransfersSyncronizer::getSubscription(const AccountPublicAddress& acc) {
  auto it = m_consumers.find(acc.viewPublicKey);
  return (it == m_consumers.end()) ? 0 : it->second->getSubscription(acc);
}

void TransfersSyncronizer::save(std::ostream& os) {
  m_sync.save(os);

  StdOutputStream stream(os);
  CryptoNote::BinaryOutputStreamSerializer s(stream);
  s(const_cast<uint32_t&>(TRANSFERS_STORAGE_ARCHIVE_VERSION), "version");

  size_t subscriptionCount = m_consumers.size();

  s.beginArray(subscriptionCount, "consumers");

  for (const auto& consumer : m_consumers) {
    s.beginObject("");
    s(const_cast<PublicKey&>(consumer.first), "view_key");

    std::stringstream consumerState;
    // synchronization state
    m_sync.getConsumerState(consumer.second.get())->save(consumerState);

    std::string blob = consumerState.str();
    s(blob, "state");
    
    std::vector<AccountPublicAddress> subscriptions;
    consumer.second->getSubscriptions(subscriptions);
    size_t subCount = subscriptions.size();

    s.beginArray(subCount, "subscriptions");

    for (auto& addr : subscriptions) {
      auto sub = consumer.second->getSubscription(addr);
      if (sub != nullptr) {
        s.beginObject("");

        std::stringstream subState;
        assert(sub);
        sub->getContainer().save(subState);
        // store data block
        std::string blob = subState.str();
        s(addr, "address");
        s(blob, "state");

        s.endObject();
      }
    }

    s.endArray();
    s.endObject();
  }
}

namespace {
std::string getObjectState(IStreamSerializable& obj) {
  std::stringstream stream;
  obj.save(stream);
  return stream.str();
}

void setObjectState(IStreamSerializable& obj, const std::string& state) {
  std::stringstream stream(state);
  obj.load(stream);
}

}

void TransfersSyncronizer::load(std::istream& is) {
  m_sync.load(is);

  StdInputStream inputStream(is);
  CryptoNote::BinaryInputStreamSerializer s(inputStream);
  uint32_t version = 0;

  s(version, "version");

  if (version > TRANSFERS_STORAGE_ARCHIVE_VERSION) {
    throw std::runtime_error("TransfersSyncronizer version mismatch");
  }


  struct ConsumerState {
    PublicKey viewKey;
    std::string state;
    std::vector<std::pair<AccountPublicAddress, std::string>> subscriptionStates;
  };

  std::vector<ConsumerState> updatedStates;

  try {
    size_t subscriptionCount = 0;
    s.beginArray(subscriptionCount, "consumers");

    while (subscriptionCount--) {
      s.beginObject("");
      PublicKey viewKey;
      s(viewKey, "view_key");

      std::string blob;
      s(blob, "state");

      auto subIter = m_consumers.find(viewKey);
      if (subIter != m_consumers.end()) {
        auto consumerState = m_sync.getConsumerState(subIter->second.get());
        assert(consumerState);

        {
          // store previous state
          auto prevConsumerState = getObjectState(*consumerState);
          // load consumer state
          setObjectState(*consumerState, blob);
          updatedStates.push_back(ConsumerState{ viewKey, std::move(prevConsumerState) });
        }

        // load subscriptions
        size_t subCount = 0;
        s.beginArray(subCount, "subscriptions");

        while (subCount--) {
          s.beginObject("");

          AccountPublicAddress acc;
          std::string state;

          s(acc, "address");
          s(state, "state");

          auto sub = subIter->second->getSubscription(acc);

          if (sub != nullptr) {
            auto prevState = getObjectState(sub->getContainer());
            setObjectState(sub->getContainer(), state);
            updatedStates.back().subscriptionStates.push_back(std::make_pair(acc, prevState));
          }

          s.endObject();
        }
        s.endArray();
      }
    }

    s.endObject();
    s.endArray();

  } catch (...) {
    // rollback state
    for (const auto& consumerState : updatedStates) {
      auto consumer = m_consumers.find(consumerState.viewKey)->second.get();
      setObjectState(*m_sync.getConsumerState(consumer), consumerState.state);
      for (const auto& sub : consumerState.subscriptionStates) {
        setObjectState(consumer->getSubscription(sub.first)->getContainer(), sub.second);
      }
    }
    throw;
  }

}

}
