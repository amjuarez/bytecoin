// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
