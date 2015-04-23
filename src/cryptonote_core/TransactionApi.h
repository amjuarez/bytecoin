// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <memory>
#include "ITransaction.h"

namespace cryptonote {
  struct Transaction;
}

namespace CryptoNote {
  std::unique_ptr<ITransaction> createTransaction();
  std::unique_ptr<ITransaction> createTransaction(const Blob& transactionBlob);
  std::unique_ptr<ITransaction> createTransaction(const cryptonote::Transaction& tx);
}
