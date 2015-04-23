// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "CommonTypes.h"
#include "IStreamSerializable.h"
#include "serialization/ISerializer.h"
#include <vector>
#include <map>

namespace CryptoNote {

class SynchronizationState : public IStreamSerializable {
public:

  struct CheckResult {
    bool detachRequired;
    uint64_t detachHeight;
    bool hasNewBlocks;
    uint64_t newBlockHeight;
  };

  typedef std::list<crypto::hash> ShortHistory;

  explicit SynchronizationState(const crypto::hash& genesisBlockHash) {
    m_blockchain.push_back(genesisBlockHash);
  }

  ShortHistory getShortHistory() const;
  CheckResult checkInterval(const BlockchainInterval& interval) const;

  void detach(uint64_t height);
  void addBlocks(const crypto::hash* blockHashes, uint64_t height, size_t count);
  uint64_t getHeight() const;

  // IStreamSerializable
  virtual void save(std::ostream& os) override;
  virtual void load(std::istream& in) override;

  // serialization
  cryptonote::ISerializer& serialize(cryptonote::ISerializer& s, const std::string& name);

private:

  std::vector<crypto::hash> m_blockchain;
};

}
