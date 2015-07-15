// Copyright (c) 2012-2015, The CryptoNote developers, The Bytecoin developers
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

#include "ICryptonoteProtocolQueryStub.h"

bool ICryptonoteProtocolQueryStub::addObserver(CryptoNote::ICryptonoteProtocolObserver* observer) {
  return false;
}

bool ICryptonoteProtocolQueryStub::removeObserver(CryptoNote::ICryptonoteProtocolObserver* observer) {
  return false;
}

uint64_t ICryptonoteProtocolQueryStub::getObservedHeight() const {
  return observedHeight;
}

size_t ICryptonoteProtocolQueryStub::getPeerCount() const {
  return peers;
}

bool ICryptonoteProtocolQueryStub::isSynchronized() const {
  return synchronized;
}

void ICryptonoteProtocolQueryStub::setPeerCount(uint32_t count) {
  peers = count;
}

void ICryptonoteProtocolQueryStub::setObservedHeight(uint64_t height) {
  observedHeight = height;
}

void ICryptonoteProtocolQueryStub::setSynchronizedStatus(bool status) {
    synchronized = status;
}
