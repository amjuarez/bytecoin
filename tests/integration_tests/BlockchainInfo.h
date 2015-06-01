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

#include "serialization/SerializationOverloads.h"
#include "cryptonote_core/cryptonote_serialization.h"

namespace CryptoNote {
  void serialize(BlockCompleteEntry& v, const std::string& name, ISerializer& s) {
    s.beginObject(name);
    s(v.blockHash, "hash");
    s.binary(v.block, "block");
    s(v.txs, "transactions");
    s.endObject();
  }

  bool operator == (const BlockCompleteEntry& a, const BlockCompleteEntry& b) {
    return
      a.blockHash == b.blockHash &&
      a.block == b.block &&
      a.txs == b.txs;
  }

  struct BlockchainInfo {
    std::vector<BlockCompleteEntry> blocks;
    std::unordered_map<crypto::hash, std::vector<uint64_t>> globalOutputs;

    bool operator == (const BlockchainInfo& other) const {
      return blocks == other.blocks && globalOutputs == other.globalOutputs;
    }

    void serialize(ISerializer& s, const std::string& name) {
      s.beginObject(name);
      s(blocks, "blocks");
      s(globalOutputs, "outputs");
      s.endObject();
    }
  };

  void storeBlockchainInfo(const std::string& filename, BlockchainInfo& bc) {
    JsonOutputStreamSerializer s;
    s(bc, "");
    std::ofstream jsonBlocks(filename, std::ios::trunc);
    jsonBlocks << s;
  }

  void loadBlockchainInfo(const std::string& filename, BlockchainInfo& bc) {
    std::ifstream jsonBlocks(filename);
    JsonInputStreamSerializer s(jsonBlocks);
    s(bc, "");
  }


}
