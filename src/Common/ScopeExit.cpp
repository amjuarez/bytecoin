// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ScopeExit.h"

namespace Tools {

ScopeExit::ScopeExit(std::function<void()>&& handler) :
  m_handler(std::move(handler)),
  m_cancelled(false) {
}

ScopeExit::~ScopeExit() {
  if (!m_cancelled) {
    m_handler();
  }
}

void ScopeExit::cancel() {
  m_cancelled = true;
}

}
