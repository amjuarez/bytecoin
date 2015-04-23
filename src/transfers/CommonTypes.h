// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <array>
#include <memory>
#include <cstdint>

#include <boost/optional.hpp>

#include "INode.h"
#include "ITransaction.h"

namespace CryptoNote {

struct BlockchainInterval {
  uint64_t startHeight;
  std::vector<crypto::hash> blocks;
};

struct CompleteBlock {
  crypto::hash blockHash;
  boost::optional<cryptonote::Block> block;
  // first transaction is always coinbase
  std::list<std::shared_ptr<ITransactionReader>> transactions;
};

}
