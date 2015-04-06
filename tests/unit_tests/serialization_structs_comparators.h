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

template <typename T>
class Comparator {
public:
  static bool compare(const T& t1, const T& t2) { return t1 == t2; }
};

namespace cryptonote {

bool operator==(const cryptonote::TransactionOutputToScript& t1, const cryptonote::TransactionOutputToScript& t2) {
 return true;
}

bool operator==(const cryptonote::TransactionOutputMultisignature& t1, const cryptonote::TransactionOutputMultisignature& t2) {
  if (t1.keys != t2.keys) {
    return false;
  }

  return t1.requiredSignatures == t2.requiredSignatures;
}

bool operator==(const cryptonote::TransactionInputGenerate& t1, const cryptonote::TransactionInputGenerate& t2) {
  return t1.height == t2.height;
}

bool operator==(const cryptonote::TransactionInputToScript& t1, const cryptonote::TransactionInputToScript& t2) {
  return true;
}

bool operator==(const cryptonote::TransactionInputToScriptHash& t1, const cryptonote::TransactionInputToScriptHash& t2) {
  return true;
}

bool operator==(const cryptonote::TransactionInputToKey& t1, const cryptonote::TransactionInputToKey& t2) {
  if (t1.amount != t2.amount) {
    return false;
  }

  if (t1.keyOffsets != t2.keyOffsets) {
    return false;
  }

  return t1.keyImage == t2.keyImage;
}

bool operator==(const cryptonote::TransactionInputMultisignature& t1, const cryptonote::TransactionInputMultisignature& t2) {
  if (t1.amount != t2.amount) {
    return false;
  }

  if (t1.signatures != t2.signatures) {
    return false;
  }

  return t1.outputIndex == t2.outputIndex;
}

bool operator==(const cryptonote::TransactionOutputToScriptHash& t1, const cryptonote::TransactionOutputToScriptHash& t2) {
  return true;
}

bool operator==(const cryptonote::TransactionOutputToKey& t1, const cryptonote::TransactionOutputToKey& t2) {
  return t1.key == t2.key;
}

bool operator==(const cryptonote::TransactionOutput& t1, const cryptonote::TransactionOutput& t2) {
  if (t1.amount != t2.amount) {
    return false;
  }

  return t1.target == t2.target;
}

bool operator==(const cryptonote::ParentBlock& t1, const cryptonote::ParentBlock& t2) {
  if (t1.majorVersion != t2.majorVersion) {
    return false;
  }

  if (t1.minorVersion != t2.minorVersion) {
    return false;
  }

  if (t1.prevId != t2.prevId) {
    return false;
  }

  if (t1.numberOfTransactions != t2.numberOfTransactions) {
    return false;
  }

  if (t1.minerTxBranch != t2.minerTxBranch) {
    return false;
  }

  if (!(t1.minerTx == t2.minerTx)) {
    return false;
  }

  return t1.blockchainBranch == t2.blockchainBranch;
}

bool operator==(const cryptonote::AccountPublicAddress& t1, const cryptonote::AccountPublicAddress& t2) {
  if (t1.m_spendPublicKey != t2.m_spendPublicKey) {
    return false;
  }

  return t1.m_viewPublicKey == t2.m_viewPublicKey;
}

bool operator==(const cryptonote::tx_extra_merge_mining_tag& t1, const cryptonote::tx_extra_merge_mining_tag& t2) {
  if (t1.depth != t2.depth) {
    return false;
  }

  return t1.merkle_root == t2.merkle_root;
}

}
