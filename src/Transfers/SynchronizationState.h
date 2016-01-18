// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "CommonTypes.h"
#include "IStreamSerializable.h"
#include "Serialization/ISerializer.h"
#include <vector>
#include <map>

namespace CryptoNote {

class SynchronizationState : public IStreamSerializable {
public:

  struct CheckResult {
    bool detachRequired;
    uint32_t detachHeight;
    bool hasNewBlocks;
    uint32_t newBlockHeight;
  };

  typedef std::vector<Crypto::Hash> ShortHistory;

  explicit SynchronizationState(const Crypto::Hash& genesisBlockHash) {
    m_blockchain.push_back(genesisBlockHash);
  }

  ShortHistory getShortHistory(uint32_t localHeight) const;
  CheckResult checkInterval(const BlockchainInterval& interval) const;

  void detach(uint32_t height);
  void addBlocks(const Crypto::Hash* blockHashes, uint32_t height, uint32_t count);
  uint32_t getHeight() const;
  const std::vector<Crypto::Hash>& getKnownBlockHashes() const;

  // IStreamSerializable
  virtual void save(std::ostream& os) override;
  virtual void load(std::istream& in) override;

  // serialization
  CryptoNote::ISerializer& serialize(CryptoNote::ISerializer& s, const std::string& name);

private:

  std::vector<Crypto::Hash> m_blockchain;
};

}
