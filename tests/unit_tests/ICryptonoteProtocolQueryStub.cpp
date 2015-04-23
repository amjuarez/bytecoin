// Copyright (c) 2011-2015 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ICryptonoteProtocolQueryStub.h"

bool ICryptonoteProtocolQueryStub::addObserver(cryptonote::ICryptonoteProtocolObserver* observer) {
  return false;
}

bool ICryptonoteProtocolQueryStub::removeObserver(cryptonote::ICryptonoteProtocolObserver* observer) {
  return false;
}

uint64_t ICryptonoteProtocolQueryStub::getObservedHeight() const {
  return observedHeight;
}

size_t ICryptonoteProtocolQueryStub::getPeerCount() const {
  return peers;
}

void ICryptonoteProtocolQueryStub::setPeerCount(uint32_t count) {
  peers = count;
}

void ICryptonoteProtocolQueryStub::setObservedHeight(uint64_t height) {
  observedHeight = height;
}
