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

#include <cstdint>

#include "cryptonote_protocol/ICryptonoteProtocolObserver.h"
#include "cryptonote_protocol/ICryptonoteProtocolQuery.h"

class ICryptonoteProtocolQueryStub: public cryptonote::ICryptonoteProtocolQuery {
public:
  ICryptonoteProtocolQueryStub() : peers(0), observedHeight(0) {}

  virtual bool addObserver(cryptonote::ICryptonoteProtocolObserver* observer);
  virtual bool removeObserver(cryptonote::ICryptonoteProtocolObserver* observer);
  virtual uint64_t getObservedHeight() const;
  virtual size_t getPeerCount() const;
  void setPeerCount(uint32_t count);
  void setObservedHeight(uint64_t height);

private:
  size_t peers;
  uint64_t observedHeight;
};

