// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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

bool operator==(const cryptonote::AccountPublicAddress& t1, const cryptonote::AccountPublicAddress& t2) {
  if (t1.m_spendPublicKey != t2.m_spendPublicKey) {
    return false;
  }

  return t1.m_viewPublicKey == t2.m_viewPublicKey;
}

}
