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

#include "cryptonote_core/cryptonote_basic.h"

namespace CryptoNote {

  struct BlockInfo {
    uint64_t height;
    crypto::hash id;

    BlockInfo() {
      clear();
    }

    void clear() {
      height = 0;
      id = cryptonote::null_hash;
    }

    bool empty() const {
      return id == cryptonote::null_hash;
    }
  };

  class ITransactionValidator {
  public:
    virtual ~ITransactionValidator() {}
    
    virtual bool checkTransactionInputs(const cryptonote::Transaction& tx, BlockInfo& maxUsedBlock) = 0;
    virtual bool checkTransactionInputs(const cryptonote::Transaction& tx, BlockInfo& maxUsedBlock, BlockInfo& lastFailed) = 0;
    virtual bool haveSpentKeyImages(const cryptonote::Transaction& tx) = 0;
  };

}
