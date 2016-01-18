// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ICryptoNoteProtocolQueryStub.h"

bool ICryptoNoteProtocolQueryStub::addObserver(CryptoNote::ICryptoNoteProtocolObserver* observer) {
  return false;
}

bool ICryptoNoteProtocolQueryStub::removeObserver(CryptoNote::ICryptoNoteProtocolObserver* observer) {
  return false;
}

uint32_t ICryptoNoteProtocolQueryStub::getObservedHeight() const {
  return observedHeight;
}

size_t ICryptoNoteProtocolQueryStub::getPeerCount() const {
  return peers;
}

bool ICryptoNoteProtocolQueryStub::isSynchronized() const {
  return synchronized;
}

void ICryptoNoteProtocolQueryStub::setPeerCount(uint32_t count) {
  peers = count;
}

void ICryptoNoteProtocolQueryStub::setObservedHeight(uint32_t height) {
  observedHeight = height;
}

void ICryptoNoteProtocolQueryStub::setSynchronizedStatus(bool status) {
    synchronized = status;
}
