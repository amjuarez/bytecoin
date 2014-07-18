// Copyright (c) 2011-2014 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <boost/optional.hpp>
#include <memory>
#include <vector>
#include <list>
#include <unordered_map>

#include "crypto/crypto.h"
#include "cryptonote_core/cryptonote_basic.h"

namespace CryptoNote {

struct TransactionContextInfo
{
  std::vector<size_t> requestedOuts;
  std::vector<uint64_t> globalIndices;
  cryptonote::transaction transaction;
  crypto::public_key transactionPubKey;
};

struct SynchronizationState
{
  SynchronizationState() : blockIdx(0), transactionIdx(0), minersTxProcessed(false) {}
  size_t blockIdx; //block index within context->new_blocks array to be processed
  size_t transactionIdx; //tx index within the block to be processed
  bool minersTxProcessed; //is miner's tx in the block processed
};

struct SynchronizationContext
{
  std::list<cryptonote::block_complete_entry> newBlocks;
  uint64_t startHeight;
  std::unordered_map<crypto::hash, TransactionContextInfo> transactionContext;
  SynchronizationState progress;
};

} //namespace CryptoNote
