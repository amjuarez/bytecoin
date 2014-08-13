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
