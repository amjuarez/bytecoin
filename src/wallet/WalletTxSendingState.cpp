// Copyright (c) 2011-2014 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "WalletTxSendingState.h"

namespace CryptoNote {

void WalletTxSendingState::sending(TransactionId id) {
  m_states[id] = SENDING;
}

void WalletTxSendingState::sent(TransactionId id) {
  m_states.erase(id);
}

void WalletTxSendingState::error(TransactionId id) {
  m_states[id] = ERRORED;
}

WalletTxSendingState::State WalletTxSendingState::state(TransactionId id) {
  auto it = m_states.find(id);

  if (it == m_states.end())
    return NOT_FOUND;

  return it->second;
}
} //namespace CryptoNote
