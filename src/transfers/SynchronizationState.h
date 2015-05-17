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
